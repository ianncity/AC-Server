/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <ctime>

#include "AdvaiEconomy.h"
#include "AdvaiMgr.h"
#include "AdvaiTradeTerms.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "WorldSession.h"

namespace Advai
{
bool GiftOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    Player* peer = ObjectAccessor::FindPlayer(_recipient);
    if (!bot || !peer || bot == peer || !manager.Applies(bot) || !bot->IsAlive() || !peer->IsAlive() ||
        !bot->IsInWorld() || !peer->IsInWorld() || bot->IsInCombat() || peer->IsInCombat() || !bot->GetGroup() ||
        bot->GetGroup() != peer->GetGroup() || bot->GetMap() != peer->GetMap() || !bot->InSamePhase(peer) ||
        !bot->IsWithinDistInMap(peer, INTERACTION_DISTANCE) || bot->GetTrader() || peer->GetTrader())
        return false;
    if (!manager.Active(_bot.GetCounter()))
        return false;
    Item* item = bot->GetItemByGuid(_item);
    if (!item || item->IsEquipped() || item->IsInTrade() || !item->CanBeTraded() ||
        manager.ItemReserved(_bot.GetCounter(), item->GetEntry()) ||
        peer->CanUseItem(item->GetTemplate()) != EQUIP_ERR_OK)
        return false;
    if (!manager.PrepareGift(_bot.GetCounter(), _recipient.GetCounter(), item->GetEntry(), item->GetCount(),
                             manager.Applies(peer)))
        return false;
    auto offer = manager.Offer(_bot.GetCounter());
    return offer && TradeOperation(_bot, *offer).Execute();
}

bool CancelTradeOperation::Execute()
{
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->GetSession())
        return false;
    if (bot->GetTrader() && bot->GetTrader()->GetGUID().GetCounter() == _peer)
    {
        // The active trader, not a previous chat partner, owns this interaction.
        if (_notifyPeer)
            bot->Whisper("Let's agree on what to trade first.", LANG_UNIVERSAL, bot->GetTrader());
        WorldPacket packet(CMSG_CANCEL_TRADE);
        bot->GetSession()->HandleCancelTradeOpcode(packet);
    }
    return true;
}

bool TradeOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    Player* peer = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(_offer.Peer));
    if (!bot || !bot->GetSession() || !manager.Applies(bot))
        return false;
    auto current = manager.Offer(_bot.GetCounter());
    if (!current || !current->Agreed || current->ByMail || current->Peer != _offer.Peer || current->Item != _offer.Item ||
        current->Quantity != _offer.Quantity || current->Copper != _offer.Copper)
        return false;
    if (!peer || !peer->IsInWorld() || !peer->GetSession() || !peer->IsAlive() || !bot->IsInWorld() ||
        !bot->IsAlive() || bot->IsInCombat() || peer->IsInCombat() ||
        static_cast<Timestamp>(time(nullptr)) >= _offer.Expires)
    {
        if (bot->GetTrader() && bot->GetTrader()->GetGUID().GetCounter() == _offer.Peer)
        {
            WorldPacket cancel(CMSG_CANCEL_TRADE);
            bot->GetSession()->HandleCancelTradeOpcode(cancel);
        }
        manager.FinishTrade(_bot.GetCounter(), false, "trade expired or participant unavailable");
        return false;
    }
    if (bot->GetMap() != peer->GetMap() || !bot->IsWithinDistInMap(peer, INTERACTION_DISTANCE))
        return false;
    if (!bot->GetTrader())
    {
        if (peer->GetTrader())
            return false;
        WorldPacket initiate(CMSG_INITIATE_TRADE);
        initiate << peer->GetGUID();
        bot->GetSession()->HandleInitiateTradeOpcode(initiate);
        if (bot->GetTrader() != peer || peer->GetTrader() != bot)
        {
            manager.FinishTrade(_bot.GetCounter(), false, "trade initiation rejected by normal player checks", &_offer);
            return false;
        }
        if (manager.Applies(peer))
        {
            auto theirs = manager.Offer(_offer.Peer);
            if (theirs && theirs->Agreed && theirs->Peer == _bot.GetCounter() && theirs->Item == _offer.Item &&
                theirs->Copper == _offer.Copper && theirs->Quantity == _offer.Quantity &&
                theirs->Selling != _offer.Selling)
            {
                WorldPacket begin(CMSG_BEGIN_TRADE);
                peer->GetSession()->HandleBeginTradeOpcode(begin);
            }
        }
        return true;
    }
    if (bot->GetTrader() != peer || !bot->GetTradeData() || !peer->GetTradeData())
        return false;
    if (_offer.Selling)
    {
        Item* selected = nullptr;
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(bot))
            for (Item* item : ai->GetInventoryAndEquippedItems())
                if (item && item->GetEntry() == _offer.Item && item->GetCount() == _offer.Quantity &&
                    item->CanBeTraded() && !item->IsEquipped())
                {
                    selected = item;
                    break;
                }
        if (!selected)
        {
            manager.FinishTrade(_bot.GetCounter(), false, "quoted stack is no longer available");
            return false;
        }
        constexpr TradeSlots itemSlot = static_cast<TradeSlots>(0);
        if (bot->GetTradeData()->GetItem(itemSlot) != selected)
        {
            WorldPacket put(CMSG_SET_TRADE_ITEM);
            put << uint8(itemSlot) << selected->GetBagSlot() << selected->GetSlot();
            bot->GetSession()->HandleSetTradeItemOpcode(put);
        }
        if (peer->GetTradeData()->GetMoney() < _offer.Copper)
            return true;
    }
    else
    {
        if (bot->GetMoney() < _offer.Copper)
        {
            manager.FinishTrade(_bot.GetCounter(), false, "quoted purchase is no longer affordable");
            return false;
        }
        uint32 quantity = 0;
        for (uint8 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
            if (Item* item = peer->GetTradeData()->GetItem(static_cast<TradeSlots>(slot));
                item && item->GetEntry() == _offer.Item)
                quantity += item->GetCount();
        if (quantity != _offer.Quantity)
            return true;
        if (bot->GetTradeData()->GetMoney() != _offer.Copper)
        {
            WorldPacket gold(CMSG_SET_TRADE_GOLD);
            gold << _offer.Copper;
            bot->GetSession()->HandleSetTradeGoldOpcode(gold);
        }
    }
    TradeContents own, theirs;
    own.Copper = bot->GetTradeData()->GetMoney();
    theirs.Copper = peer->GetTradeData()->GetMoney();
    for (uint8 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
    {
        if (Item* item = bot->GetTradeData()->GetItem(static_cast<TradeSlots>(slot)))
            own.Items[slot] = {item->GetEntry(), item->GetCount()};
        if (Item* item = peer->GetTradeData()->GetItem(static_cast<TradeSlots>(slot)))
            theirs.Items[slot] = {item->GetEntry(), item->GetCount()};
    }
    if (!MatchesTrade(_offer, own, theirs))
        return true;
    WorldPacket accept(CMSG_ACCEPT_TRADE);
    accept << uint32(0);
    bot->GetSession()->HandleAcceptTradeOpcode(accept);
    return true;
}
}  // namespace Advai
