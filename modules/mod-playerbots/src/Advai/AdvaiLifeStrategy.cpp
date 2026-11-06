/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiLifeStrategy.h"

#include <algorithm>
#include <ctime>
#include <limits>
#include <stdexcept>

#include "AdvaiEconomy.h"
#include "AdvaiMgr.h"
#include "AdvaiSocial.h"
#include "AdvaiSupplies.h"
#include "AiObjectContext.h"
#include "DBCStores.h"
#include "Event.h"
#include "GameObject.h"
#include "GatherNodeMgr.h"
#include "Group.h"
#include "ItemUsageValue.h"
#include "NewRpgDoGather.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotFactory.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Random.h"
#include "SocialMgr.h"
#include "Trainer.h"
#include "WorldPacket.h"
#include "WorldSession.h"

std::vector<NextAction> AdvaiLifeStrategy::getDefaultActions()
{
    return {NextAction("advai arena preparation", ACTION_DEFAULT + 0.8f),
            NextAction("advai raid", ACTION_DEFAULT + 0.75f), NextAction("advai life", ACTION_DEFAULT + 0.5f),
            NextAction("advai leisure", ACTION_DEFAULT + 0.25f)};
}

bool AdvaiLifeAction::isUseful()
{
    if (!Advai::Manager::Instance().Applies(bot) || !botAI->AllowActivity() || bot->IsInCombat() ||
        bot->IsNonMeleeSpellCast(false) || static_cast<Advai::Timestamp>(time(nullptr)) < _nextAttempt)
        return false;
    auto goal = Advai::Manager::Instance().PlannedGoal(bot->GetGUID().GetCounter());
    if (!goal)
        return false;
    if ((botAI->HasGameClientMaster() ||
         (bot->GetGroup() &&
          (bot->GetMap()->IsDungeon() || Advai::Manager::Instance().QueuedActivity(bot->GetGUID().GetCounter())))) &&
        *goal != Advai::Goal::Trade)
        return false;
    switch (*goal)
    {
        case Advai::Goal::Repair:
        case Advai::Goal::Sell:
        case Advai::Goal::Restock:
        case Advai::Goal::Train:
        case Advai::Goal::Craft:
        case Advai::Goal::Bank:
        case Advai::Goal::Auction:
        case Advai::Goal::Mail:
        case Advai::Goal::Trade:
            return true;
        default:
            return false;
    }
}

bool AdvaiLifeAction::Execute(Event /*event*/)
{
    auto& manager = Advai::Manager::Instance();
    auto planned = manager.PlannedGoal(bot->GetGUID().GetCounter());
    if (!planned)
        return false;
    Advai::Goal goal = *planned;
    _nextAttempt = static_cast<Advai::Timestamp>(time(nullptr)) + 2;
    if (bot->IsInFlight())
        return true;
    if (auto supplies = manager.Supplies(bot->GetGUID().GetCounter());
        goal == Advai::Goal::Restock && supplies && supplies->RecipeItem)
    {
        bool used = botAI->DoSpecificAction("use", Event("advai", std::to_string(supplies->RecipeItem)), true);
        manager.Report(bot->GetGUID().GetCounter(),
                       {goal, used ? Advai::Outcome::Started : Advai::Outcome::Blocked,
                        used ? "using owned recipe; awaiting native learned spell"
                             : "owned recipe failed native use requirements"});
        _nextAttempt += 10;
        return used;
    }
    if (goal == Advai::Goal::Trade)
    {
        auto offer = manager.Offer(bot->GetGUID().GetCounter());
        if (!offer || !offer->Agreed)
            return false;
        if (offer->ByMail)
            goal = Advai::Goal::Mail;
        else
        {
            Player* peer = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(offer->Peer));
            if (!peer || peer->GetMap() != bot->GetMap() || static_cast<Advai::Timestamp>(time(nullptr)) >= offer->Expires)
            {
                manager.FinishTrade(bot->GetGUID().GetCounter(), false, "meeting unavailable or trade expired");
                return false;
            }
            if (!bot->IsWithinDistInMap(peer, INTERACTION_DISTANCE))
            {
                if (botAI->HasGameClientMaster())
                    return false;
                return MoveFarTo(WorldPosition(peer));
            }
            return PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<Advai::TradeOperation>(bot->GetGUID(), *offer));
        }
    }
    if (goal == Advai::Goal::Restock)
        if (auto supplies = manager.Supplies(bot->GetGUID().GetCounter());
            supplies && supplies->NeedsMount && supplies->MountItem)
        {
            bool used = botAI->DoSpecificAction("use", Event("advai", std::to_string(supplies->MountItem)), true);
            manager.Report(bot->GetGUID().GetCounter(),
                           {goal, used ? Advai::Outcome::Started : Advai::Outcome::Blocked,
                            used ? "using owned mount item; awaiting learned riding spell"
                                 : "owned mount item failed native use requirements"});
            _nextAttempt += 10;
            return used;
        }
    uint32 flags = UNIT_NPC_FLAG_VENDOR;
    char const* action = "buy";
    std::string param = "vendor";
    switch (goal)
    {
        case Advai::Goal::Repair:
            flags = UNIT_NPC_FLAG_REPAIR;
            action = "repair";
            param.clear();
            break;
        case Advai::Goal::Sell:
            action = "sell";
            param = "vendor";
            break;
        case Advai::Goal::Restock:
            break;
        case Advai::Goal::Train:
            flags = UNIT_NPC_FLAG_TRAINER;
            action = "trainer";
            param = "learn";
            break;
        case Advai::Goal::Craft:
            action = "craft random item";
            param.clear();
            break;
        case Advai::Goal::Bank:
            flags = UNIT_NPC_FLAG_BANKER;
            action = "bank";
            param = "trade goods";
            break;
        case Advai::Goal::Auction:
            flags = UNIT_NPC_FLAG_AUCTIONEER;
            break;
        case Advai::Goal::Mail:
            flags = Advai::SERVICE_MAILBOX;
            break;
        default:
            return false;
    }
    if (goal == Advai::Goal::Restock)
    {
        auto supplies = manager.Supplies(bot->GetGUID().GetCounter());
        bool bank = supplies && !supplies->BankSupply.IsEmpty();
        if (bank != _restockBank)
        {
            _destination.reset();
            _journey.Reset();
            _restockBank = bank;
        }
        if (bank)
            flags = UNIT_NPC_FLAG_BANKER;
    }
    if (goal == Advai::Goal::Craft)
    {
        if (_recipe && _craftPending && Advai::RecipeCompleted(bot, *_recipe))
        {
            if (!_craftParents.empty())
            {
                _recipe = _craftParents.back();
                _craftParents.pop_back();
                _ingredient = 0;
                _craftPending = false;
                _destination.reset();
                manager.Report(bot->GetGUID().GetCounter(),
                               {goal, Advai::Outcome::Started, "crafted prerequisite observed; resuming parent recipe"});
                return true;
            }
            manager.Report(bot->GetGUID().GetCounter(),
                           {goal, Advai::Outcome::Succeeded, "crafted product or equipment enchant observed"});
            manager.ReserveCraft(bot->GetGUID().GetCounter(), 0, {});
            _recipe.reset();
            _ingredient = 0;
            _craftPending = false;
            return true;
        }
        if (!_recipe)
        {
            auto committed = manager.CraftRecipe(bot->GetGUID().GetCounter());
            _recipe = committed ? Advai::RestoreRecipe(botAI, committed) : Advai::ChooseRecipe(botAI);
        }
        if (!_recipe || !Advai::RefreshRecipe(bot, *_recipe))
        {
            manager.Report(bot->GetGUID().GetCounter(),
                           {goal, Advai::Outcome::Blocked, "no useful known recipe satisfies profession goals"});
            _recipe.reset();
            _craftParents.clear();
            manager.ReserveCraft(bot->GetGUID().GetCounter(), 0, {});
            return false;
        }
        auto reservations = Advai::RecipeItems(*_recipe);
        for (auto const& parent : _craftParents)
        {
            auto items = Advai::RecipeItems(parent);
            reservations.insert(reservations.end(), items.begin(), items.end());
        }
        manager.ReserveCraft(bot->GetGUID().GetCounter(),
                             _craftParents.empty() ? _recipe->Spell : _craftParents.front().Spell,
                             std::move(reservations));
        if (_recipe->MissingItem && manager.Delivery(bot->GetGUID().GetCounter()))
        {
            manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Blocked,
                                                         "craft is waiting for a previously purchased mail delivery"});
            _nextAttempt += 30;
            return false;
        }
        if (_recipe->MissingItem)
            if (auto prerequisite = Advai::RecipeForItem(bot, _recipe->MissingItem))
            {
                bool cycle = prerequisite->Spell == _recipe->Spell ||
                    std::any_of(_craftParents.begin(), _craftParents.end(), [&](auto const& parent)
                                { return parent.Spell == prerequisite->Spell; });
                if (cycle || _craftParents.size() >= 8)
                {
                    manager.Report(bot->GetGUID().GetCounter(),
                                   {goal, Advai::Outcome::Blocked, "craft prerequisite cycle or depth limit reached"});
                    _recipe.reset();
                    _craftParents.clear();
                    return false;
                }
                _craftParents.push_back(*_recipe);
                _recipe = *prerequisite;
                _ingredient = 0;
                _destination.reset();
                manager.Report(bot->GetGUID().GetCounter(),
                               {goal, Advai::Outcome::Started, "preparing a known recipe for a required tool or material"});
                return true;
            }
        if (!_recipe->MissingItem)
        {
            if (auto* gathering = std::get_if<NewRpgInfo::DoGather>(&botAI->rpgInfo.data);
                gathering && gathering->requiredItem)
                botAI->rpgInfo.ChangeToIdle();
            if (!Advai::HasCraftingStation(botAI, *_recipe))
            {
                auto now = static_cast<Advai::Timestamp>(time(nullptr));
                if (_craftStationSpell != _recipe->Spell)
                {
                    _craftStationSpell = _recipe->Spell;
                    _craftStation.reset();
                }
                if (!_craftStation)
                {
                    std::vector<Advai::ServiceLocation> excluded;
                    for (auto const& blocked : _blockedServices)
                        if (blocked.Until > now)
                            excluded.push_back(blocked.Location);
                    _craftStation = Advai::FindCraftingStation(bot, *_recipe, excluded);
                }
                if (!_craftStation)
                {
                    manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Blocked,
                        "known recipe requires a forge, anvil, laboratory or other focus with no accessible native station"});
                    return false;
                }
                bool isNear = _craftStation->Map == bot->GetMapId() &&
                    bot->GetExactDist(_craftStation->X, _craftStation->Y, _craftStation->Z) <= INTERACTION_DISTANCE;
                bool moved = !isNear && _journey.Travel(botAI,
                    WorldPosition(_craftStation->Map, _craftStation->X, _craftStation->Y, _craftStation->Z), goal,
                    [this](WorldPosition const& point) { return MoveFarTo(point); });
                if (moved)
                {
                    manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Started,
                                                                 "traveling to the real crafting station required by the recipe"});
                    return true;
                }
                if (_blockedServices.size() >= 128)
                    _blockedServices.pop_front();
                _blockedServices.push_back({*_craftStation, now + 300});
                _craftStation.reset();
                manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Blocked,
                    isNear ? "crafting focus unavailable at its indexed location; replanning another real station"
                         : "crafting station route failed; replanning through legitimate travel"});
                return false;
            }
            bot->SetSelection(bot->GetGUID());
            bool result;
            if (_recipe->Enchantment)
            {
                Item* target = bot->GetItemByGuid(_recipe->TargetItem);
                result = target && botAI->CanCastSpell(_recipe->Spell, bot, true, target) &&
                         botAI->CastSpell(_recipe->Spell, bot, target);
            }
            else
                result = botAI->DoSpecificAction("cast custom spell", Event("advai", std::to_string(_recipe->Spell)), true);
            _craftPending = result;
            manager.Report(bot->GetGUID().GetCounter(),
                           {goal, result ? Advai::Outcome::Started : Advai::Outcome::Blocked,
                            result ? "craft cast started; awaiting actual product"
                                   : "craft failed spell, tool or reagent checks"});
            _nextAttempt += 10;
            return result;
        }
        // Keep the craft goal and its reserved materials while using the existing
        // native gathering/loot actions to acquire its actual missing reagent.
        auto now = static_cast<Advai::Timestamp>(time(nullptr));
        bool bankHasMaterial = bot->GetItemCount(_recipe->MissingItem, true) >
                               bot->GetItemCount(_recipe->MissingItem, false);
        if (!bankHasMaterial && !_recipe->MissingTool)
        {
            auto* gathering = std::get_if<NewRpgInfo::DoGather>(&botAI->rpgInfo.data);
            if (gathering && gathering->requiredItem && gathering->requiredItem != _recipe->MissingItem)
            {
                botAI->rpgInfo.ChangeToIdle();
                gathering = nullptr;
            }
            if (gathering && gathering->requiredItem == _recipe->MissingItem)
                return NewRpgDoGatherAction(botAI).Execute(Event("advai"));
            if (_gatherIngredient != _recipe->MissingItem || now >= _gatherRetryAfter)
            {
                _gatherIngredient = _recipe->MissingItem;
                _gatherRetryAfter = now + 600;
                std::unordered_set<ObjectGuid::LowType> visited;
                if (auto* node = sGatherNodeMgr.GetNextNode(bot, visited, _recipe->MissingItem))
                {
                    botAI->rpgInfo.ChangeToDoGather();
                    auto& acquisition = std::get<NewRpgInfo::DoGather>(botAI->rpgInfo.data);
                    acquisition.requiredItem = _recipe->MissingItem;
                    acquisition.requiredCount = bot->GetItemCount(_recipe->MissingItem, false) + _recipe->MissingCount;
                    acquisition.expiresAt = static_cast<uint32>(now + 180);
                    acquisition.nodeSpawnId = node->spawnId;
                    acquisition.nodePos = node->pos;
                    manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Started,
                        "gathering a required crafting material from eligible real resource nodes"});
                    return true;
                }
            }
        }
        if (_ingredient != _recipe->MissingItem || _recipe->Source == Advai::IngredientSource::Bank)
        {
            _ingredient = _recipe->MissingItem;
            _destination.reset();
            if (bot->GetItemCount(_ingredient, true) > bot->GetItemCount(_ingredient, false))
                _recipe->Source = Advai::IngredientSource::Bank;
            else
            {
                bool vendorFound = false;
                for (auto const& service : manager.Services())
                    if (auto* faction = sFactionTemplateStore.LookupEntry(service.Faction);
                        service.Map == bot->GetMapId() && (service.Flags & UNIT_NPC_FLAG_VENDOR) && faction &&
                        !bot->GetFactionTemplateEntry()->IsHostileTo(*faction))
                        if (auto* stock = sObjectMgr->GetNpcVendorItemList(service.Entry))
                            for (auto* offered : stock->m_items)
                                if (offered && offered->item == _ingredient && !offered->ExtendedCost)
                                    vendorFound = true;
                _recipe->Source = vendorFound ? Advai::IngredientSource::Vendor : Advai::IngredientSource::Auction;
            }
        }
        flags = _recipe->Source == Advai::IngredientSource::Bank      ? UNIT_NPC_FLAG_BANKER
                : _recipe->Source == Advai::IngredientSource::Auction ? UNIT_NPC_FLAG_AUCTIONEER
                                                                      : UNIT_NPC_FLAG_VENDOR;
    }
    if (_serviceGoal != goal)
    {
        _destination.reset();
        _serviceGoal = goal;
        _journey.Reset();
    }
    if (!_destination)
    {
        auto supplies = goal == Advai::Goal::Restock ? manager.Supplies(bot->GetGUID().GetCounter()) : nullptr;
        if (goal == Advai::Goal::Restock && !supplies)
        {
            manager.Report(bot->GetGUID().GetCounter(),
                           {goal, Advai::Outcome::Blocked, "supply facts unavailable; awaiting inventory snapshot"});
            return false;
        }
        auto now = static_cast<Advai::Timestamp>(time(nullptr));
        std::erase_if(_blockedServices, [now](auto const& service) { return service.Until <= now; });
        float nearest = std::numeric_limits<float>::max();
        for (auto const& service : manager.Services())
        {
            bool homeService = service.Map != bot->GetMapId() && service.Map == bot->m_homebindMapId &&
                               bot->GetItemCount(6948, false) && !bot->HasSpellCooldown(8690) &&
                               WorldPosition(bot->m_homebindMapId, bot->m_homebindX, bot->m_homebindY, bot->m_homebindZ)
                                       .GetExactDist(service.X, service.Y, service.Z) < 1500.0f;
            bool connected = service.Map != bot->GetMapId() &&
                             Advai::HasTravelConnection(bot->GetMapId(), service.Map);
            if ((service.Map != bot->GetMapId() && !homeService && !connected) || !(service.Flags & flags))
                continue;
            if (supplies && !_restockBank && !Advai::StocksNeededSupplies(bot, service.Entry, *supplies))
                continue;
            if (goal == Advai::Goal::Craft && _recipe->Source == Advai::IngredientSource::Vendor)
            {
                auto* stock = sObjectMgr->GetNpcVendorItemList(service.Entry);
                if (!stock ||
                    std::none_of(
                        stock->m_items.begin(), stock->m_items.end(), [&](auto* offered)
                        { return offered && offered->item == _recipe->MissingItem && !offered->ExtendedCost; }))
                    continue;
            }
            if (std::any_of(_blockedServices.begin(), _blockedServices.end(),
                            [&](auto const& blocked)
                            {
                                return blocked.Location.Entry == service.Entry && blocked.Location.Map == service.Map &&
                                       blocked.Location.X == service.X && blocked.Location.Y == service.Y;
                            }))
                continue;
            auto* faction = sFactionTemplateStore.LookupEntry(service.Faction);
            if (goal != Advai::Goal::Mail && (!faction || bot->GetFactionTemplateEntry()->IsHostileTo(*faction)))
                continue;
            if (goal == Advai::Goal::Train)
            {
                auto* trainer = sObjectMgr->GetTrainer(service.Entry);
                if (!trainer || !trainer->IsTrainerValidForPlayer(bot))
                    continue;
                bool useful = false;
                for (auto const& spell : trainer->GetSpells())
                    if (spell.MoneyCost <= bot->GetMoney() && trainer->CanTeachSpell(bot, &spell) &&
                        PlayerbotFactory::IsTrainerSpellAllowedForBot(bot, trainer, &spell))
                    {
                        useful = true;
                        break;
                    }
                if (!useful)
                    continue;
            }
            float distance = homeService ? 2000.0f + WorldPosition(bot->m_homebindMapId, bot->m_homebindX,
                                                                   bot->m_homebindY, bot->m_homebindZ)
                                                         .GetExactDist(service.X, service.Y, service.Z)
                             : service.Map != bot->GetMapId() ? 20000.0f
                                         : bot->GetExactDist(service.X, service.Y, service.Z);
            if (distance < nearest)
            {
                nearest = distance;
                _destination = service;
            }
        }
        if (!_destination)
        {
            manager.Report(
                bot->GetGUID().GetCounter(),
                {goal, Advai::Outcome::Blocked, "no eligible service has a known ground, portal, transport or home route"});
            return false;
        }
    }
    WorldPosition destination(_destination->Map, _destination->X, _destination->Y, _destination->Z);
    if (destination.GetMapId() != bot->GetMapId())
    {
        if (destination.GetMapId() == bot->m_homebindMapId && bot->GetItemCount(6948, false) &&
            !bot->HasSpellCooldown(8690) && WorldPosition(bot->m_homebindMapId, bot->m_homebindX,
                bot->m_homebindY, bot->m_homebindZ).GetExactDist(destination) < 1500.0f)
        {
            bool cast = botAI->DoSpecificAction("hearthstone", Event("advai"), true);
            manager.Report(bot->GetGUID().GetCounter(),
                           {goal, cast ? Advai::Outcome::Started : Advai::Outcome::Blocked,
                            cast ? "casting the owned hearthstone to reach a service near home"
                                 : "home service route failed the hearthstone's native use checks"});
            _nextAttempt += 12;
            return cast;
        }
        return _journey.Travel(botAI, destination, goal,
                               [this](WorldPosition const& point) { return MoveFarTo(point); });
    }
    if (bot->GetExactDist(destination) > INTERACTION_DISTANCE)
    {
        if (bot->GetExactDist(destination) < 25.0f && goal != Advai::Goal::Mail)
            for (auto guid : context->GetValue<GuidVector>("nearest npcs")->Get())
                if (auto* npc = botAI->GetCreature(guid); npc && npc->IsAlive() &&
                    npc->GetEntry() == _destination->Entry && bot->InSamePhase(npc) && bot->CanSeeOrDetect(npc))
                    return MoveWorldObjectTo(guid, INTERACTION_DISTANCE * 0.8f);
        manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Started, "travelling to service"});
        bool moved = _journey.Travel(botAI, destination, goal,
                                    [this](WorldPosition const& point) { return MoveFarTo(point); });
        if (!manager.PlannedGoal(bot->GetGUID().GetCounter()))
        {
            _blockedServices.push_back({*_destination, static_cast<Advai::Timestamp>(time(nullptr)) + 600});
            if (_blockedServices.size() > 32)
                _blockedServices.pop_front();
            _destination.reset();
        }
        return moved;
    }
    if (goal == Advai::Goal::Mail)
    {
        GuidVector objects = context->GetValue<GuidVector>("nearest game objects")->Get();
        for (ObjectGuid guid : objects)
            if (GameObject* mailbox = botAI->GetGameObject(guid); mailbox &&
                                                                  mailbox->GetGoType() == GAMEOBJECT_TYPE_MAILBOX &&
                                                                  bot->IsWithinDistInMap(mailbox, INTERACTION_DISTANCE))
            {
                auto offer = manager.Offer(bot->GetGUID().GetCounter());
                bool queued = offer && offer->ByMail && offer->Agreed && offer->Selling
                    ? PlayerbotWorldThreadProcessor::instance().QueueOperation(
                        std::make_unique<Advai::SendCodOperation>(bot->GetGUID(), guid, *offer))
                    : PlayerbotWorldThreadProcessor::instance().QueueOperation(
                        std::make_unique<Advai::CollectMailOperation>(bot->GetGUID(), guid));
                _nextAttempt += 30;
                if (!queued)
                    manager.Report(bot->GetGUID().GetCounter(),
                                   {goal, Advai::Outcome::Blocked, "mail operation queue full"});
                return queued;
            }
        _destination.reset();
        manager.Report(bot->GetGUID().GetCounter(),
                       {goal, Advai::Outcome::Blocked, "mailbox unavailable at destination"});
        return false;
    }
    GuidVector nearby = context->GetValue<GuidVector>("nearest npcs")->Get();
    for (ObjectGuid guid : nearby)
        if (Creature* creature = botAI->GetCreature(guid);
            creature && creature->GetEntry() == _destination->Entry && bot->GetNPCIfCanInteractWith(guid, flags))
        {
            bot->SetSelection(guid);
            if (goal == Advai::Goal::Craft)
            {
                if (_recipe->Source == Advai::IngredientSource::Bank)
                {
                    auto ingredients = Advai::BankIngredients(bot, *_recipe);
                    if (ingredients.empty())
                    {
                        manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Blocked,
                                                                     "required bank ingredients are no longer stored"});
                        return false;
                    }
                    bool queued =
                        PlayerbotWorldThreadProcessor::instance().QueueOperation(std::make_unique<Advai::BankOperation>(
                            bot->GetGUID(), guid, std::move(ingredients), true, goal));
                    _nextAttempt += 10;
                    if (!queued)
                        manager.Report(bot->GetGUID().GetCounter(),
                                       {goal, Advai::Outcome::Blocked, "ingredient withdrawal queue full"});
                    return queued;
                }
                auto persona = manager.Persona(bot->GetGUID().GetCounter());
                std::uint32_t budget =
                    static_cast<std::uint32_t>(bot->GetMoney() * (25ULL - persona.Frugality / 5) / 100);
                bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
                    std::make_unique<Advai::BuyIngredientOperation>(bot->GetGUID(), guid, *_recipe, budget));
                _nextAttempt += 30;
                if (!queued)
                    manager.Report(bot->GetGUID().GetCounter(),
                                   {goal, Advai::Outcome::Blocked, "ingredient purchase queue full"});
                return queued;
            }
            if (goal == Advai::Goal::Restock)
            {
                if (_restockBank)
                {
                    auto plan = Advai::CaptureSupplies(botAI, Advai::ClassReagents(bot));
                    if (plan.BankSupply.IsEmpty())
                    {
                        _destination.reset();
                        manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Started,
                            "bank supply requirement changed; refreshing preparation facts"});
                        return true;
                    }
                    bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
                        std::make_unique<Advai::BankOperation>(bot->GetGUID(), guid,
                            std::vector<ObjectGuid>{plan.BankSupply}, true, goal));
                    _nextAttempt += 5;
                    if (!queued)
                        manager.Report(bot->GetGUID().GetCounter(), {goal, Advai::Outcome::Blocked,
                                                                     "supply withdrawal queue full"});
                    return queued;
                }
                bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
                    std::make_unique<Advai::BuySuppliesOperation>(bot->GetGUID(), guid));
                _destination.reset();
                _nextAttempt += 10;
                if (!queued)
                    manager.Report(bot->GetGUID().GetCounter(),
                                   {goal, Advai::Outcome::Blocked, "supply purchase queue full"});
                return queued;
            }
            if (goal == Advai::Goal::Auction)
            {
                for (Item* item : botAI->GetInventoryAndEquippedItems())
                    if (item && item->CanBeTraded() && !item->IsEquipped() && !item->IsInTrade() &&
                        context->GetValue<ItemUsage>("item usage", std::to_string(item->GetEntry()))->Get() ==
                            ITEM_USAGE_AH)
                    {
                        bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
                            std::make_unique<Advai::AuctionOperation>(
                                bot->GetGUID(), guid, item->GetGUID(),
                                manager.Persona(bot->GetGUID().GetCounter()).Frugality));
                        _nextAttempt += 30;
                        if (!queued)
                            manager.Report(bot->GetGUID().GetCounter(),
                                           {goal, Advai::Outcome::Blocked, "auction operation queue full"});
                        return queued;
                    }
                manager.Report(bot->GetGUID().GetCounter(),
                               {goal, Advai::Outcome::Blocked, "no tradeable auction stock"});
                return false;
            }
            auto measure = [&]
            {
                std::pair<std::uint64_t, std::uint64_t> totals{};
                for (Item* item : botAI->GetInventoryAndEquippedItems())
                    if (item)
                    {
                        totals.first += item->GetCount();
                        totals.second += item->GetUInt32Value(ITEM_FIELD_DURABILITY);
                    }
                return totals;
            };
            auto before = (goal == Advai::Goal::Restock || goal == Advai::Goal::Repair)
                              ? measure()
                              : std::pair<std::uint64_t, std::uint64_t>{};
            bool result = botAI->DoSpecificAction(action, Event("advai", param), true);
            if (result && (goal == Advai::Goal::Restock || goal == Advai::Goal::Repair))
            {
                auto after = measure();
                bool changed = goal == Advai::Goal::Restock ? after.first > before.first : after.second > before.second;
                if (!changed)
                {
                    manager.Report(bot->GetGUID().GetCounter(),
                                   {goal, Advai::Outcome::Blocked,
                                    goal == Advai::Goal::Restock
                                        ? "supplier supplied no affordable useful items"
                                        : "repair service changed no durability; check funds and damaged items"});
                    result = false;
                }
            }
            _nextAttempt += result ? 10 : 30;
            return result;
        }
    _destination.reset();
    manager.Report(bot->GetGUID().GetCounter(),
                   {goal, Advai::Outcome::Blocked, "service NPC unavailable at destination"});
    return false;
}

bool AdvaiLeisureAction::isUseful()
{
    if (!Advai::Manager::Instance().Applies(bot) || !botAI->AllowActivity() || !bot->IsAlive() || bot->IsInCombat() ||
        bot->IsNonMeleeSpellCast(false) || bot->GetGroupInvite() || bot->GetTradeData() || bot->IsInFlight() ||
        bot->IsFlying() || bot->IsFalling() || bot->IsInWater() || bot->GetMap()->IsBattlegroundOrArena() ||
        bot->InBattlegroundQueue() || botAI->HasGameClientMaster() ||
        static_cast<Advai::Timestamp>(time(nullptr)) < _nextCheck)
        return false;
    auto goal = Advai::Manager::Instance().PlannedGoal(bot->GetGUID().GetCounter());
    return goal &&
           (*goal == Advai::Goal::Rest ||
            (*goal == Advai::Goal::Socialize && !bot->GetGroup() && !bot->GetMap()->IsDungeon())) &&
           !Advai::Manager::Instance().QueuedActivity(bot->GetGUID().GetCounter());
}

bool AdvaiLeisureAction::Execute(Event /*event*/)
{
    auto& manager = Advai::Manager::Instance();
    auto planned = manager.PlannedGoal(bot->GetGUID().GetCounter());
    if (!planned || (*planned != Advai::Goal::Rest && *planned != Advai::Goal::Socialize))
        return false;
    auto now = static_cast<Advai::Timestamp>(time(nullptr));
    _nextCheck = now + 2;
    if (_goal != *planned)
    {
        _goal = *planned;
        _pauseUntil = 0;
        _inn.reset();
    }
    auto persona = manager.Persona(bot->GetGUID().GetCounter());
    if (_goal == Advai::Goal::Rest)
    {
        bot->StopMoving();
        if (bot->IsMounted())
        {
            WorldPacket cancel(CMSG_CANCEL_MOUNT_AURA);
            bot->GetSession()->HandleCancelMountAuraOpcode(cancel);
        }
        auto supplies = manager.Supplies(bot->GetGUID().GetCounter());
        char const* action = "sit";
        if (supplies && bot->GetShapeshiftForm() == FORM_NONE)
        {
            if (bot->GetHealthPct() < 90 && supplies->Food)
                action = "food";
            else if (supplies->UsesMana && bot->GetPowerPct(POWER_MANA) < 85 && supplies->Drink)
                action = "drink";
        }
        bool restored = botAI->DoSpecificAction(action, Event("advai"), true);
        manager.Report(bot->GetGUID().GetCounter(),
                       {_goal, restored ? Advai::Outcome::Started : Advai::Outcome::Blocked,
                        restored ? "resting through normal regeneration or owned supplies"
                                 : "rest action failed its ordinary movement, form or item checks"});
        return restored;
    }
    _nextCheck = now;
    SetDuration(500 + persona.Patience * 5);
    if (!_pauseUntil && !_inn && now >= _nextInnVisit)
    {
        _nextInnVisit = now + (10 + persona.Seed % 20) * 60;
        float nearest = 400.0f;
        for (auto const& service : manager.Services())
            if (service.Map == bot->GetMapId() && (service.Flags & UNIT_NPC_FLAG_INNKEEPER))
                if (auto* faction = sFactionTemplateStore.LookupEntry(service.Faction);
                    faction && !bot->GetFactionTemplateEntry()->IsHostileTo(*faction))
                    if (float distance = bot->GetExactDist(service.X, service.Y, service.Z); distance < nearest)
                    {
                        nearest = distance;
                        _inn = service;
                    }
    }
    if (_inn && bot->GetExactDist(_inn->X, _inn->Y, _inn->Z) > INTERACTION_DISTANCE)
    {
        manager.Report(bot->GetGUID().GetCounter(),
                       {_goal, Advai::Outcome::Started, "walking to a nearby inn for a social pause"});
        bool moved = MoveFarTo(WorldPosition(_inn->Map, _inn->X, _inn->Y, _inn->Z));
        if (!manager.PlannedGoal(bot->GetGUID().GetCounter()))
            _inn.reset();
        return moved;
    }
    if (!_pauseUntil)
    {
        bot->StopMoving();
        _pauseUntil = now + 10 + persona.Patience / 5 + urand(0, 10);
        // Do not greet the instant a pause starts; with hundreds of bots that reads as everyone waving.
        _nextGesture = std::max(_nextGesture, now + 5 + urand(0, 20));
        manager.Report(bot->GetGUID().GetCounter(), {_goal, Advai::Outcome::Started, "taking a short social pause"});
    }
    if (now >= _pauseUntil)
    {
        manager.Report(bot->GetGUID().GetCounter(), {_goal, Advai::Outcome::Succeeded, "social pause finished"});
        _pauseUntil = 0;
        _inn.reset();
        _nextCheck = now + 15;
        return true;
    }
    if (now >= _nextGesture)
    {
        _nextGesture = now + 60 + (100 - persona.Sociability) + urand(0, 120);
        // Only the more sociable personas gesture, and at most once per player per half hour.
        if (urand(0, 99) >= persona.Sociability / 4)
            return true;
        for (auto itr = _gestured.begin(); itr != _gestured.end();)
            itr = itr->second <= now ? _gestured.erase(itr) : std::next(itr);
        auto nearby = context->GetValue<GuidVector>("nearest friendly players")->Get();
        for (auto guid : nearby)
            if (Player* other = ObjectAccessor::GetPlayer(bot->GetMap(), guid);
                other && other != bot && !_gestured.count(guid.GetCounter()) && !other->IsInCombat() &&
                bot->InSamePhase(other) &&
                bot->CanSeeOrDetect(other) && bot->IsWithinDistInMap(other, 15.0f) &&
                !(other->GetSocial() && other->GetSocial()->HasIgnore(bot->GetGUID())) &&
                !(bot->GetSocial() && bot->GetSocial()->HasIgnore(other->GetGUID())))
            {
                _gestured[guid.GetCounter()] = now + 1800;
                bot->SetFacingToObject(other);
                bot->HandleEmoteCommand(persona.SpeechStyle < 50 ? EMOTE_ONESHOT_WAVE : EMOTE_ONESHOT_BOW);
                return true;
            }
    }
    return true;
}

bool AdvaiArenaPreparationAction::isUseful()
{
    auto& manager = Advai::Manager::Instance();
    if (!manager.Applies(bot) || !botAI->AllowActivity() || !bot->IsAlive() || bot->IsInCombat() ||
        bot->GetGroup() || bot->GetGroupInvite() || bot->GetTradeData() || bot->IsInFlight() ||
        bot->IsBeingTeleported() || bot->IsNonMeleeSpellCast(false) || bot->InBattlegroundQueue() ||
        bot->GetMap()->IsDungeon() || bot->InBattleground() || botAI->HasGameClientMaster() ||
        static_cast<Advai::Timestamp>(time(nullptr)) < _nextCheck ||
        manager.PlannedGoal(bot->GetGUID().GetCounter()) != Advai::Goal::Arena)
        return false;
    for (std::uint8_t slot = 0; slot < MAX_ARENA_SLOT; ++slot)
        if (bot->GetArenaTeamId(slot))
            return false;
    return true;
}

bool AdvaiArenaPreparationAction::Execute(Event /*event*/)
{
    auto& manager = Advai::Manager::Instance();
    auto id = bot->GetGUID().GetCounter();
    _nextCheck = static_cast<Advai::Timestamp>(time(nullptr)) + 3;
    if (!_registrar)
    {
        float best = std::numeric_limits<float>::max();
        for (auto const& service : manager.Services())
            if ((service.Flags & UNIT_NPC_FLAG_PETITIONER) && !(service.Flags & UNIT_NPC_FLAG_TABARDDESIGNER) &&
                Advai::HasTravelConnection(bot->GetMapId(), service.Map))
                if (auto* faction = sFactionTemplateStore.LookupEntry(service.Faction);
                    faction && !bot->GetFactionTemplateEntry()->IsHostileTo(*faction))
                {
                    float distance = service.Map == bot->GetMapId()
                        ? bot->GetExactDist(service.X, service.Y, service.Z) : 20000.0f;
                    if (distance < best)
                    {
                        best = distance;
                        _registrar = service;
                    }
                }
        if (!_registrar)
        {
            manager.Report(id, {Advai::Goal::Arena, Advai::Outcome::Blocked, "no accessible native arena registrar"});
            return false;
        }
    }
    if (bot->GetMapId() != _registrar->Map ||
        bot->GetExactDist(_registrar->X, _registrar->Y, _registrar->Z) > INTERACTION_DISTANCE)
        return _journey.Travel(botAI, WorldPosition(_registrar->Map, _registrar->X, _registrar->Y, _registrar->Z),
                              Advai::Goal::Arena, [this](WorldPosition const& point) { return MoveFarTo(point); });
    for (auto guid : context->GetValue<GuidVector>("nearest npcs")->Get())
        if (auto* npc = botAI->GetCreature(guid); npc && npc->GetEntry() == _registrar->Entry &&
            bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER))
        {
            _nextCheck += 30;
            bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<Advai::ArenaCharterOperation>(bot->GetGUID(), guid));
            if (!queued)
                manager.Report(id, {Advai::Goal::Arena, Advai::Outcome::Blocked, "arena charter operation queue is full"});
            return queued;
        }
    _registrar.reset();
    manager.Report(id, {Advai::Goal::Arena, Advai::Outcome::Blocked, "arena registrar unavailable at its location"});
    return false;
}

bool AdvaiRaidAction::isUseful()
{
    auto& manager = Advai::Manager::Instance();
    return manager.Applies(bot) && botAI->AllowActivity() && bot->IsAlive() && !bot->IsInCombat() &&
           !botAI->HasGameClientMaster() && !bot->GetMap()->IsDungeon() && !bot->IsInFlight() &&
           !bot->IsBeingTeleported() && !bot->GetTradeData() && !bot->IsNonMeleeSpellCast(false) &&
           static_cast<Advai::Timestamp>(time(nullptr)) >= _nextCheck &&
           manager.PlannedGoal(bot->GetGUID().GetCounter()) == Advai::Goal::Raid &&
           manager.RaidTarget(bot->GetGUID().GetCounter());
}

bool AdvaiRaidAction::Execute(Event /*event*/)
{
    auto& manager = Advai::Manager::Instance();
    auto id = bot->GetGUID().GetCounter();
    auto now = static_cast<Advai::Timestamp>(time(nullptr));
    _nextCheck = now + 3;
    auto target = manager.RaidTarget(id);
    auto* dungeon = sLFGDungeonStore.LookupEntry(target);
    if (!target || !dungeon)
        return false;
    if (_dungeon != target)
    {
        _dungeon = target;
        _entrance.reset();
        _journey.Reset();
        _nextRoster = 0;
    }
    auto* group = bot->GetGroup();
    bool leader = !group || group->GetLeaderGUID() == bot->GetGUID();
    if (leader && now >= _nextRoster)
    {
        _nextRoster = now + 20;
        bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
            std::make_unique<Advai::RaidAssemblyOperation>(bot->GetGUID(), target));
        if (!queued)
            manager.Report(id, {Advai::Goal::Raid, Advai::Outcome::Blocked, "raid assembly operation queue is full"});
        return queued;
    }
    if (!group || !group->isRaidGroup() || !manager.RaidReady(id))
        return false;
    if (!_entrance)
    {
        float nearest = std::numeric_limits<float>::max();
        for (auto const& entrance : manager.RaidEntrances(dungeon->MapID))
            if (Advai::HasTravelConnection(bot->GetMapId(), entrance.Map))
                if (auto distance = entrance.Map == bot->GetMapId()
                                        ? bot->GetExactDist(entrance.X, entrance.Y, entrance.Z) : 20000.0f;
                    distance < nearest || (distance == nearest && _entrance && entrance.Entry < _entrance->Entry))
                {
                    nearest = distance;
                    _entrance = entrance;
                }
        if (!_entrance)
        {
            manager.Report(id, {Advai::Goal::Raid, Advai::Outcome::Blocked,
                                "no known native route connects the raid entrance"});
            return false;
        }
    }
    auto* trigger = sObjectMgr->GetAreaTrigger(_entrance->Entry);
    if (!trigger)
        throw std::runtime_error("ADVAI selected raid entrance disappeared from the native catalog");
    if (bot->IsInAreaTriggerRadius(trigger))
    {
        bool queued = PlayerbotWorldThreadProcessor::instance().QueueOperation(
            std::make_unique<Advai::EnterRaidOperation>(bot->GetGUID(), target, _entrance->Entry));
        if (!queued)
            manager.Report(id, {Advai::Goal::Raid, Advai::Outcome::Blocked, "raid entry operation queue is full"});
        return queued;
    }
    if (!leader && bot->GetGroup()->GetLeaderGUID() != bot->GetGUID())
    {
        auto* leaderPlayer = ObjectAccessor::FindPlayer(bot->GetGroup()->GetLeaderGUID());
        // Follow nearby members on land; separated members use their own legitimate journey.
        if (leaderPlayer && leaderPlayer->GetMap() == bot->GetMap() &&
            leaderPlayer->GetMapId() != dungeon->MapID && !leaderPlayer->GetTransport() &&
            bot->IsWithinDistInMap(leaderPlayer, 60.0f))
            return false;
    }
    bool moved = _journey.Travel(botAI, WorldPosition(_entrance->Map, _entrance->X, _entrance->Y, _entrance->Z),
                                Advai::Goal::Raid, [this](WorldPosition const& point) { return MoveFarTo(point); });
    if (!moved)
        _entrance.reset();
    return moved;
}
