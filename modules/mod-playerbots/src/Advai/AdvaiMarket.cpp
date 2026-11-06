/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiMarket.h"

#include <algorithm>
#include <ctime>
#include <mutex>
#include <map>
#include <stdexcept>
#include <tuple>

#include "AuctionHouseMgr.h"
#include "AuctionHouseScript.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotsDatabase.h"

namespace Advai
{
namespace
{
struct SaleWindow
{
    Timestamp Hour = 0;
    std::uint64_t Units = 0;
    std::uint64_t Revenue = 0;
};
struct Sales
{
    std::array<SaleWindow, 24> Hours{};
    Timestamp Last = 0;
};
struct PendingSale
{
    std::uint64_t Units = 0;
    std::uint64_t Revenue = 0;
    Timestamp Last = 0;
};
struct House
{
    std::uint32_t Id = 0;
    std::unordered_map<std::uint32_t, MarketQuote> Published;
    std::unordered_map<std::uint32_t, MarketQuote> Building;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> Listings;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> BuildingListings;
    std::unordered_map<std::uint32_t, std::size_t> ListingOffsets;
    std::unordered_map<std::uint32_t, Sales> Transactions;
    std::uint32_t Cursor = 0;
    Timestamp Next = 0;
    bool Scanning = false;
    Timestamp PublishedAt = 0;
    std::map<std::tuple<std::uint32_t, std::uint32_t, Timestamp>, PendingSale> DirtySales;
};
std::mutex marketMutex;
std::unordered_map<AuctionHouseObject*, House> houses;

class MarketScript final : public AuctionHouseScript
{
public:
    MarketScript() : AuctionHouseScript("AdvaiMarketScript") {}

    void OnBeforeAuctionHouseMgrSendAuctionSuccessfulMail(
        AuctionHouseMgr*, AuctionEntry* entry, Player*, std::uint32_t&, std::uint32_t&, bool&, bool&, bool&) override
    {
        if (!sPlayerbotAIConfig.AdvaiEnabled || !entry || !entry->bidder || !entry->bid ||
            !entry->itemCount || entry->bidder == entry->owner)
            return;
        // This hook covers immediate buyouts as well as auctions which end with a bidder.
        auto now = static_cast<Timestamp>(time(nullptr));
        std::lock_guard lock(marketMutex);
        auto& sale = houses[sAuctionMgr->GetAuctionsMapByHouseId(entry->houseId)]
                         .Transactions[entry->item_template];
        auto hour = now / 3600;
        auto& window = sale.Hours[hour % sale.Hours.size()];
        if (window.Hour != hour)
            window = {hour, 0};
        window.Units += entry->itemCount;
        window.Revenue += entry->bid;
        sale.Last = now;
        auto& house = houses[sAuctionMgr->GetAuctionsMapByHouseId(entry->houseId)];
        if (!house.Id)
            throw std::runtime_error("ADVAI sale observed before market initialization");
        // Preserve the sale's original house even when native cross-faction settings merge the views.
        auto& pending = house.DirtySales[{static_cast<std::uint32_t>(entry->houseId), entry->item_template, hour}];
        pending.Units += entry->itemCount;
        pending.Revenue += entry->bid;
        pending.Last = now;
    }
};
}  // namespace

void InitializeMarket(Timestamp now)
{
    std::lock_guard lock(marketMutex);
    for (auto id : {AuctionHouseId::Alliance, AuctionHouseId::Horde, AuctionHouseId::Neutral})
    {
        auto& house = houses[sAuctionMgr->GetAuctionsMapByHouseId(id)];
        if (!house.Id)
            house.Id = static_cast<std::uint32_t>(id);
    }
    if (!PlayerbotsDatabase.Query("SELECT COUNT(*) FROM playerbots_advai_market_sales"))
        throw std::runtime_error("ADVAI market history unavailable; apply the market migration");
    if (auto rows = PlayerbotsDatabase.Query(
            "SELECT house,item,hour,units,revenue,last_sale FROM playerbots_advai_market_sales WHERE hour>={}",
            now / 3600 > 23 ? now / 3600 - 23 : 0))
        do
        {
            auto* fields = rows->Fetch();
            auto hour = fields[2].Get<Timestamp>();
            if (hour > now / 3600)
                throw std::runtime_error("ADVAI market history contains a future sale bucket");
            auto storedHouse = fields[0].Get<std::uint32_t>();
            if (storedHouse != static_cast<std::uint32_t>(AuctionHouseId::Alliance) &&
                storedHouse != static_cast<std::uint32_t>(AuctionHouseId::Horde) &&
                storedHouse != static_cast<std::uint32_t>(AuctionHouseId::Neutral))
                throw std::runtime_error("ADVAI market history contains an invalid house");
            auto& house = houses[sAuctionMgr->GetAuctionsMapByHouseId(static_cast<AuctionHouseId>(storedHouse))];
            auto& sale = house.Transactions[fields[1].Get<std::uint32_t>()];
            auto& window = sale.Hours[hour % sale.Hours.size()];
            window.Hour = hour;
            window.Units += fields[3].Get<std::uint64_t>();
            window.Revenue += fields[4].Get<std::uint64_t>();
            sale.Last = std::max(sale.Last, fields[5].Get<Timestamp>());
        } while (rows->NextRow());
}

void UpdateMarket(Timestamp now)
{
    std::lock_guard lock(marketMutex);
    // Resolve cross-faction configuration through the native manager and avoid scanning merged houses twice.
    for (auto id : {AuctionHouseId::Alliance, AuctionHouseId::Horde, AuctionHouseId::Neutral})
        houses.try_emplace(sAuctionMgr->GetAuctionsMapByHouseId(id));
    std::size_t remaining = 1024;
    std::size_t writes = 64;
    for (auto& [object, house] : houses)
    {
        while (writes && !house.DirtySales.empty())
        {
            auto const& [key, sale] = *house.DirtySales.begin();
            auto [houseId, item, hour] = key;
            PlayerbotsDatabase.Execute(
                "INSERT INTO playerbots_advai_market_sales(house,item,hour,units,revenue,last_sale) "
                "VALUES({},{},{},{},{},{}) ON DUPLICATE KEY UPDATE units=units+VALUES(units),"
                "revenue=revenue+VALUES(revenue),last_sale=GREATEST(last_sale,VALUES(last_sale))",
                houseId, item, hour, sale.Units, sale.Revenue, sale.Last);
            house.DirtySales.erase(house.DirtySales.begin());
            --writes;
        }
        if (!remaining || now < house.Next)
            continue;
        if (!house.Scanning)
        {
            house.Building.clear();
            house.BuildingListings.clear();
            house.Cursor = 0;
            house.Scanning = true;
        }
        auto const& auctions = object->GetAuctions();
        auto it = auctions.upper_bound(house.Cursor);
        while (remaining && it != auctions.end())
        {
            auto* entry = it->second;
            house.Cursor = it->first;
            ++it;
            --remaining;
            if (!entry->itemCount || !entry->buyout || entry->expire_time <= static_cast<time_t>(now))
                continue;
            auto& quote = house.Building[entry->item_template];
            house.BuildingListings[entry->item_template].push_back(entry->Id);
            auto price = (static_cast<std::uint64_t>(entry->buyout) + entry->itemCount - 1) / entry->itemCount;
            if (!quote.UnitPrice || price < quote.UnitPrice)
                quote.UnitPrice = price;
            quote.ListedUnits += entry->itemCount;
            ++quote.Listings;
        }
        if (it != auctions.end())
            continue;
        std::erase_if(house.Transactions, [now](auto const& value)
            { return now >= value.second.Last + 24 * 3600; });
        for (auto const& [item, sale] : house.Transactions)
        {
            auto& quote = house.Building[item];
            quote.LastSale = sale.Last;
            std::uint64_t revenue = 0;
            for (auto const& window : sale.Hours)
                if (window.Hour <= now / 3600 && now / 3600 - window.Hour < 24)
                {
                    quote.SoldUnits += window.Units;
                    revenue += window.Revenue;
                }
            if (!quote.UnitPrice && quote.SoldUnits)
                quote.UnitPrice = revenue / quote.SoldUnits;
        }
        for (auto& [item, quote] : house.Building)
            quote.Observed = now;
        house.Published.swap(house.Building);
        house.Listings.swap(house.BuildingListings);
        house.BuildingListings.clear();
        std::erase_if(house.ListingOffsets, [&](auto const& offset) { return !house.Listings.contains(offset.first); });
        house.PublishedAt = now;
        house.Building.clear();
        house.Scanning = false;
        house.Next = now + 60;
    }
    static Timestamp nextCleanup = 0;
    if (now >= nextCleanup)
    {
        nextCleanup = now + 3600;
        PlayerbotsDatabase.Execute("DELETE FROM playerbots_advai_market_sales WHERE hour<{}",
                                   now / 3600 > 23 ? now / 3600 - 23 : 0);
    }
}

MarketQuote ReadMarket(std::uint32_t factionTemplate, std::uint32_t item)
{
    auto* object = sAuctionMgr->GetAuctionsMap(factionTemplate);
    std::lock_guard lock(marketMutex);
    auto house = houses.find(object);
    if (house == houses.end())
        return {};
    auto quote = house->second.Published.find(item);
    return quote == house->second.Published.end() ? MarketQuote{} : quote->second;
}

MarketListingPage ReadMarketListings(std::uint32_t factionTemplate, std::uint32_t item)
{
    auto* object = sAuctionMgr->GetAuctionsMap(factionTemplate);
    std::lock_guard lock(marketMutex);
    MarketListingPage page;
    auto house = houses.find(object);
    if (house == houses.end())
        return page;
    auto& market = house->second;
    page.Observed = market.PublishedAt;
    auto listings = market.Listings.find(item);
    if (listings == market.Listings.end() || listings->second.empty())
        return page;
    auto const& ids = listings->second;
    auto& offset = market.ListingOffsets[item];
    offset %= ids.size();
    auto count = std::min<std::size_t>(256, ids.size());
    page.Complete = count == ids.size();
    page.Auctions.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        page.Auctions.push_back(ids[(offset + i) % ids.size()]);
    offset = (offset + count) % ids.size();
    return page;
}

void AddAdvaiMarketScripts()
{
    new MarketScript();
}

std::string MarketStatistics()
{
    std::lock_guard lock(marketMutex);
    std::size_t products = 0, sales = 0, scans = 0;
    for (auto const& [object, house] : houses)
    {
        products += house.Published.size();
        sales += house.Transactions.size();
        scans += house.Scanning;
    }
    return " market_products=" + std::to_string(products) + " market_sold_products=" + std::to_string(sales) +
           " market_scans_pending=" + std::to_string(scans);
}
}  // namespace Advai
