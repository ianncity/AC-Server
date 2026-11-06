/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GiveItemAction.h"

#include "AdvaiEconomy.h"
#include "AdvaiMgr.h"
#include "Event.h"
#include "ItemCountValue.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Playerbots.h"

std::vector<std::string> split(std::string const s, char delim);

bool GiveItemAction::Execute(Event /*event*/)
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    Player* receiver = dynamic_cast<Player*>(target);
    if (!receiver)
        return false;

    PlayerbotAI* receiverAi = GET_PLAYERBOT_AI(receiver);
    bool advanced = Advai::Manager::Instance().Applies(bot);
    if (advanced && !Advai::Manager::Instance().CanOfferGift(bot->GetGUID().GetCounter()))
        return false;
    if (!receiverAi && !advanced)
        return false;

    if (receiverAi && receiverAi->GetAiObjectContext()->GetValue<uint32>("item count", item)->Get())
        return true;

    std::vector<Item*> items = InventoryAction::parseItems(item, ITERATE_ITEMS_IN_BAGS);
    for (Item* item : items)
    {
        if (receiver->CanUseItem(item->GetTemplate()) != EQUIP_ERR_OK)
            continue;

        if (advanced)
        {
            if (!bot->GetGroup() || bot->GetGroup() != receiver->GetGroup() || bot->IsInCombat() ||
                receiver->IsInCombat() || !bot->IsWithinDistInMap(receiver, INTERACTION_DISTANCE) ||
                !item->CanBeTraded() || item->IsInTrade() || item->IsEquipped() ||
                Advai::Manager::Instance().ItemReserved(bot->GetGUID().GetCounter(), item->GetEntry()))
                continue;
            return PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<Advai::GiftOperation>(bot->GetGUID(), receiver->GetGUID(), item->GetGUID()));
        }

        ItemPosCountVec dest;
        InventoryResult msg = receiver->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg == EQUIP_ERR_OK)
        {
            std::ostringstream out;
            out << "Got " << chat->FormatItem(item->GetTemplate(), item->GetCount()) << " from " << bot->GetName();

            bot->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            item->SetOwnerGUID(target->GetGUID());
            receiver->MoveItemToInventory(dest, item, true);

            receiverAi->TellMasterNoFacing(out.str());
        }
        else
        {
            std::ostringstream out;
            out << "Cannot get " << chat->FormatItem(item->GetTemplate(), item->GetCount()) << " from "
                << bot->GetName() << "- my bags are full";
            receiverAi->TellError(out.str());
        }
    }

    return true;
}

Unit* GiveItemAction::GetTarget() { return AI_VALUE2(Unit*, "party member without item", item); }

bool GiveItemAction::isUseful()
{
    return GetTarget() && AI_VALUE2(uint8, "mana", "self target") > sPlayerbotAIConfig.LowMana;
}

Unit* GiveFoodAction::GetTarget() { return AI_VALUE(Unit*, "party member without food"); }

bool GiveFoodAction::isUseful()
{
    if (!GetTarget())
        return false;

    bool isRandomBot = GetTarget()->IsPlayer() && sRandomPlayerbotMgr.IsRandomBot((Player*)GetTarget());

    return !isRandomBot || (isRandomBot && !botAI->HasCheat(BotCheatMask::food));
}

Unit* GiveWaterAction::GetTarget() { return AI_VALUE(Unit*, "party member without water"); }

bool GiveWaterAction::isUseful()
{
    if (!GetTarget())
        return false;

    bool isRandomBot = GetTarget()->IsPlayer() && sRandomPlayerbotMgr.IsRandomBot((Player*)GetTarget());

    return !isRandomBot || (isRandomBot && !botAI->HasCheat(BotCheatMask::food));
}
