/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_MARKET_H
#define PLAYERBOTS_ADVAI_MARKET_H

#include "AdvaiModel.h"

namespace Advai
{
// Quotes describe observed public auctions. An empty quote means no evidence, never free materials.
struct MarketQuote
{
    std::uint64_t UnitPrice = 0;
    std::uint64_t ListedUnits = 0;
    std::uint64_t SoldUnits = 0;
    std::uint32_t Listings = 0;
    Timestamp Observed = 0;
    Timestamp LastSale = 0;

    bool HasDemand(Timestamp now) const
    {
        return SoldUnits && now >= LastSale && now - LastSale < 24 * 3600 &&
               ListedUnits < SoldUnits * 2;
    }
};

// Update on the world thread; readers only inspect published summaries.
void InitializeMarket(Timestamp now);
void UpdateMarket(Timestamp now);
MarketQuote ReadMarket(std::uint32_t factionTemplate, std::uint32_t item);
struct MarketListingPage
{
    std::vector<std::uint32_t> Auctions;
    Timestamp Observed = 0;
    bool Complete = true;
};
MarketListingPage ReadMarketListings(std::uint32_t factionTemplate, std::uint32_t item);
std::string MarketStatistics();
void AddAdvaiMarketScripts();
}  // namespace Advai

#endif
