/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiEconomy.h"

#include <algorithm>
#include <ctime>

#include "AdvaiMgr.h"
#include "AdvaiMarket.h"
#include "AdvaiSupplies.h"
#include "AuctionHouseMgr.h"
#include "BankPackets.h"
#include "GameObject.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Playerbots.h"
#include "PlayerbotAI.h"
#include "WorldSession.h"

namespace Advai
{
bool BankOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->IsInWorld() || !bot->GetSession() || !manager.Applies(bot) ||
        !manager.Active(_bot.GetCounter()))
        return false;
    if (!bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData() ||
        !bot->GetNPCIfCanInteractWith(_banker, UNIT_NPC_FLAG_BANKER))
    {
        manager.Report(_bot.GetCounter(), {_goal, Outcome::Blocked, "banker interaction unavailable"});
        return false;
    }
    if ((_goal == Goal::Craft || _goal == Goal::Restock) && manager.PlannedGoal(_bot.GetCounter()) != _goal)
        return false;
    WorldPacket activate(CMSG_BANKER_ACTIVATE);
    activate << _banker;
    bot->GetSession()->HandleBankerActivateOpcode(activate);
    std::size_t moved = 0;
    for (auto guid : _items)
    {
        Item* item = bot->GetItemByGuid(guid);
        if (!item || item->IsInTrade() || item->IsEquipped() ||
            bot->IsBankPos(item->GetBagSlot(), item->GetSlot()) != _withdraw ||
            (!_withdraw && manager.ItemReserved(_bot.GetCounter(), item->GetEntry())))
            continue;
        auto entry = item->GetEntry();
        if (_withdraw && _goal == Goal::Restock)
        {
            auto* ai = GET_PLAYERBOT_AI(bot);
            if (!ai || !CaptureSupplies(ai, ClassReagents(bot)).Missing(bot, item->GetTemplate()))
                continue;
        }
        auto before = bot->GetItemCount(entry, false);
        WorldPackets::Bank::AutoStoreBankItem packet{WorldPacket(CMSG_AUTOSTORE_BANK_ITEM)};
        packet.Bag = item->GetBagSlot();
        packet.Slot = item->GetSlot();
        bot->GetSession()->HandleAutoStoreBankItemOpcode(packet);
        auto after = bot->GetItemCount(entry, false);
        if (_withdraw ? after > before : after < before)
            ++moved;
    }
    bool complete = moved && moved == _items.size();
    manager.Report(_bot.GetCounter(),
                   {_goal, complete ? ((_goal == Goal::Craft || _goal == Goal::Restock)
                                           ? Outcome::Started : Outcome::Succeeded) : Outcome::Blocked,
                    complete ? "bank transfers confirmed in carried inventory"
                             : "bank transfer incomplete: check bag capacity, item locations and reservations"});
    return complete;
}

bool AuctionOperation::Execute()
{
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData() ||
        !bot->GetSession() || !Manager::Instance().Applies(bot) || !Manager::Instance().Active(_bot.GetCounter()))
        return false;
    Creature* npc = bot->GetNPCIfCanInteractWith(_auctioneer, UNIT_NPC_FLAG_AUCTIONEER);
    Item* item = bot->GetItemByGuid(_item);
    if (!npc || !item || !item->CanBeTraded() || item->IsInTrade() || item->IsEquipped() || item->IsNotEmptyBag())
        return false;
    if (Manager::Instance().ItemReserved(_bot.GetCounter(), item->GetEntry()))
        return false;
    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || proto->HasFlag(ITEM_FLAG_CONJURED) || item->GetUInt32Value(ITEM_FIELD_DURATION))
        return false;
    uint32 count = item->GetCount();
    uint64 unitPrice = 0;
    auto* house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
    uint32 owned = 0, sameProduct = 0;
    for (auto const& [id, listing] : house->GetAuctions())
    {
        if (listing->owner == _bot)
        {
            ++owned;
            if (listing->item_template == item->GetEntry())
                ++sameProduct;
        }
    }
    if (owned >= 8 || sameProduct >= 2)
    {
        Manager::Instance().Report(_bot.GetCounter(), {Goal::Auction, Outcome::Blocked,
                                                       "personal auction stock limit reached; await sales or returns"});
        return false;
    }
    auto market = ReadMarket(npc->GetFaction(), item->GetEntry());
    auto now = static_cast<Timestamp>(time(nullptr));
    if (market.UnitPrice && now <= market.Observed + 300)
        unitPrice = market.UnitPrice * (market.HasDemand(now) ? 100 : 98) / 100;
    if (!unitPrice && proto->SellPrice)
        unitPrice = static_cast<uint64>(proto->SellPrice) * (200 + _frugality) / 100;
    if (!unitPrice)
    {
        Manager::Instance().Report(_bot.GetCounter(), {Goal::Auction, Outcome::Blocked,
                                                       "no observed market price or vendor valuation for item"});
        return false;
    }
    unitPrice = std::max<uint64>(unitPrice, proto->SellPrice);
    uint64 buyout = std::min<uint64>(MAX_MONEY_AMOUNT, std::max<uint64>(1, unitPrice * count));
    auto* houseEntry = AuctionHouseMgr::GetAuctionHouseEntryFromFactionTemplate(npc->GetFaction());
    if (!houseEntry || bot->GetMoney() < AuctionHouseMgr::GetAuctionDeposit(houseEntry, 12 * HOUR, item, count))
    {
        Manager::Instance().Report(_bot.GetCounter(),
                                   {Goal::Auction, Outcome::Blocked, "auction deposit unaffordable"});
        return false;
    }
    WorldPacket packet(CMSG_AUCTION_SELL_ITEM);
    packet << _auctioneer << uint32(1) << _item << count << static_cast<uint32>(std::max<uint64>(1, buyout * 9 / 10))
           << static_cast<uint32>(buyout) << uint32(12 * HOUR / MINUTE);
    uint32 before = bot->GetItemCount(item->GetEntry());
    uint32 itemId = item->GetEntry();
    bot->GetSession()->HandleAuctionSellItem(packet);
    bool posted = bot->GetItemCount(itemId) < before;
    Manager::Instance().Report(_bot.GetCounter(), {Goal::Auction, posted ? Outcome::Succeeded : Outcome::Blocked,
                                                   posted ? "owned goods posted through auction handler"
                                                          : "auction handler rejected listing"});
    return posted;
}

bool SendCodOperation::Execute()
{
    auto& manager = Manager::Instance();
    auto* bot = ObjectAccessor::FindPlayer(_bot);
    auto current = manager.Offer(_bot.GetCounter());
    if (!bot || !bot->IsInWorld() || !bot->GetSession() || !manager.Applies(bot) ||
        !manager.Active(_bot.GetCounter()) || !bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData() ||
        !current || !current->Agreed || !current->ByMail || !current->Selling || !current->Copper ||
        current->Peer != _offer.Peer || current->Item != _offer.Item ||
        current->Quantity != _offer.Quantity || current->Copper != _offer.Copper ||
        current->Expires <= static_cast<Timestamp>(time(nullptr)))
        return false;
    auto* mailbox = ObjectAccessor::GetGameObject(*bot, _mailbox);
    auto* recipient = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(_offer.Peer));
    auto* ai = GET_PLAYERBOT_AI(bot);
    if (!mailbox || mailbox->GetGoType() != GAMEOBJECT_TYPE_MAILBOX ||
        !bot->IsWithinDistInMap(mailbox, INTERACTION_DISTANCE) || !recipient || !ai)
    {
        manager.Report(_bot.GetCounter(), {Goal::Trade, Outcome::Blocked, "COD mailbox or agreed recipient unavailable"});
        return false;
    }
    Item* selected = nullptr;
    for (auto* item : ai->GetInventoryAndEquippedItems())
        if (item && !item->IsEquipped() && !item->IsInTrade() && item->CanBeTraded() &&
            item->GetEntry() == _offer.Item && item->GetCount() == _offer.Quantity)
        {
            selected = item;
            break;
        }
    if (!selected)
    {
        manager.FinishTrade(_bot.GetCounter(), false, "agreed COD stack no longer available", &_offer);
        return false;
    }
    auto itemGuid = selected->GetGUID();
    WorldPacket send(CMSG_SEND_MAIL);
    send << _mailbox << recipient->GetName() << std::string("Agreed item, cash on delivery") << std::string("");
    send << uint32(41) << uint32(0) << uint8(1) << uint8(0) << itemGuid;
    send << uint32(0) << _offer.Copper << uint64(0) << uint8(0);
    bot->GetSession()->HandleSendMail(send);
    if (bot->GetItemByGuid(itemGuid))
    {
        manager.Report(_bot.GetCounter(), {Goal::Trade, Outcome::Blocked,
            "native COD mail rejected: check postage, recipient restrictions and item mail eligibility"});
        return false;
    }
    manager.MailDispatched(_bot.GetCounter(), _offer);
    return true;
}

bool CollectMailOperation::Execute()
{
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData() ||
        !bot->GetSession() || !Manager::Instance().Applies(bot) || !Manager::Instance().Active(_bot.GetCounter()))
        return false;
    GameObject* mailbox = ObjectAccessor::GetGameObject(*bot, _mailbox);
    if (!mailbox || mailbox->GetGoType() != GAMEOBJECT_TYPE_MAILBOX ||
        !bot->IsWithinDistInMap(mailbox, INTERACTION_DISTANCE))
        return false;
    WorldPacket open(CMSG_GET_MAIL_LIST);
    open << _mailbox;
    bot->GetSession()->HandleGetMailList(open);
    std::vector<std::pair<uint32, uint32>> attachments;
    std::vector<uint32> money;
    for (Mail* mail : bot->GetMails())
    {
        if (!mail || mail->state == MAIL_STATE_DELETED || mail->deliver_time > time(nullptr))
            continue;
        if (!(mail->checked & MAIL_CHECK_MASK_READ))
        {
            WorldPacket read(CMSG_MAIL_MARK_AS_READ);
            read << _mailbox << mail->messageID;
            bot->GetSession()->HandleMailMarkAsRead(read);
        }
        if (mail->COD)
            continue;
        if (mail->money)
            money.push_back(mail->messageID);
        for (auto const& item : mail->items)
            if (attachments.size() < 12)
                attachments.emplace_back(mail->messageID, item.item_guid);
    }
    for (uint32 id : money)
    {
        WorldPacket packet(CMSG_MAIL_TAKE_MONEY);
        packet << _mailbox << id;
        bot->GetSession()->HandleMailTakeMoney(packet);
    }
    for (auto const& [mail, item] : attachments)
    {
        auto expected = Manager::Instance().Delivery(_bot.GetCounter());
        auto before = expected && expected->ItemGuid == item ? bot->GetItemCount(expected->Item, false) : 0;
        WorldPacket packet(CMSG_MAIL_TAKE_ITEM);
        packet << _mailbox << mail << item;
        bot->GetSession()->HandleMailTakeItem(packet);
        if (expected && expected->ItemGuid == item && bot->GetItemCount(expected->Item, false) > before)
            if (Mail* updated = bot->GetMail(mail);
                updated && std::none_of(updated->items.begin(), updated->items.end(),
                                        [item](auto const& attachment) { return attachment.item_guid == item; }))
                Manager::Instance().FinishDelivery(_bot.GetCounter(), item, true);
    }
    // Mail remains intact when the core rejects collection; never delete it to simulate success.
    bool outstanding = false;
    for (Mail const* mail : bot->GetMails())
        if (mail && mail->state != MAIL_STATE_DELETED && mail->deliver_time <= time(nullptr) && !mail->COD &&
            (mail->money || !mail->items.empty()))
            outstanding = true;
    if (auto expected = Manager::Instance().Delivery(_bot.GetCounter()))
    {
        Manager::Instance().Report(_bot.GetCounter(),
                                   {Goal::Mail, Outcome::Blocked,
                                    static_cast<Timestamp>(time(nullptr)) >= expected->Deadline
                                        ? "expected auction item is overdue; inspect delivery before another purchase"
                                        : "waiting for the exact purchased item to arrive and be collected"});
        return false;
    }
    Manager::Instance().Report(_bot.GetCounter(), {Goal::Mail, outstanding ? Outcome::Blocked : Outcome::Succeeded,
                                                   outstanding ? "mail remains unclaimed: check bag space or money cap"
                                                               : "delivered mail checked and collected"});
    return !outstanding;
}
}  // namespace Advai
