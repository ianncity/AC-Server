/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiProfession.h"

#include <algorithm>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <tuple>

#include "AdvaiMgr.h"
#include "AdvaiMarket.h"
#include "AdvaiTravel.h"
#include "AiObjectContext.h"
#include "AuctionHouseMgr.h"
#include "Bag.h"
#include "CharacterCache.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "ItemUsageValue.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StatsWeightCalculator.h"
#include "WorldSession.h"

namespace Advai
{
namespace
{
std::vector<std::uint32_t> professionTools;
std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> productRecipes;
struct CraftStation
{
    ServiceLocation Location;
    std::uint32_t Phase = 0;
};
std::unordered_map<std::uint32_t, std::vector<CraftStation>> craftStations;
bool toolsInitialized = false;

std::optional<RecipePlan> EnchantRecipe(Player* bot, SpellInfo const* spell)
{
    if (!bot || !spell || !spell->IsAbilityOfSkillType(SKILL_ENCHANTING))
        return std::nullopt;
    std::uint32_t enchantment = 0;
    for (auto const& effect : spell->Effects)
        if (effect.Effect == SPELL_EFFECT_ENCHANT_ITEM && effect.MiscValue > 0)
            enchantment = static_cast<std::uint32_t>(effect.MiscValue);
    if (!enchantment)
        return std::nullopt;
    StatsWeightCalculator weights(bot);
    float proposed = weights.CalculateEnchant(enchantment);
    float bestGain = 0;
    std::optional<RecipePlan> selected;
    for (std::uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (auto* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            item && item->IsFitToSpellRequirements(spell) && !item->IsInTrade())
        {
            auto existing = item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT);
            float previous = weights.CalculateEnchant(existing);
            float gain = proposed - previous;
            // Replace an enchant only for a useful upgrade; skill gains do not justify destroying it.
            if (existing == enchantment || gain <= bestGain || (existing && proposed <= previous * 1.2f))
                continue;
            RecipePlan plan;
            plan.Spell = spell->Id;
            plan.TargetItem = item->GetGUID();
            plan.Enchantment = enchantment;
            plan.BeforeEnchantment = existing;
            selected = plan;
            bestGain = gain;
        }
    return selected;
}
}

void InitializeProfessionTools()
{
    professionTools.clear();
    productRecipes.clear();
    craftStations.clear();
    for (auto const& [guid, data] : sObjectMgr->GetAllGOData())
        if (auto* object = sObjectMgr->GetGameObjectTemplate(data.id);
            object && object->type == GAMEOBJECT_TYPE_SPELL_FOCUS && object->spellFocus.focusId)
        {
            std::uint32_t faction = 0;
            if (auto* addon = sObjectMgr->GetGameObjectTemplateAddon(data.id))
                faction = addon->faction;
            craftStations[object->spellFocus.focusId].push_back(
                {{data.id, data.mapid, 0, faction, data.posX, data.posY, data.posZ}, data.phaseMask});
        }
    for (std::uint32_t id = 0; id < sSpellMgr->GetSpellInfoStoreSize(); ++id)
        if (auto* spell = sSpellMgr->GetSpellInfo(id); spell && !spell->IsPassive())
            for (auto const& effect : spell->Effects)
                if (effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.ItemType)
                    productRecipes[effect.ItemType].push_back(id);
    for (auto const& [id, item] : *sObjectMgr->GetItemTemplateStore())
        if (item.TotemCategory)
            professionTools.push_back(id);
    std::sort(professionTools.begin(), professionTools.end(), [](auto left, auto right)
              {
                  auto* a = sObjectMgr->GetItemTemplate(left);
                  auto* b = sObjectMgr->GetItemTemplate(right);
                  return std::tuple(a->RequiredSkillRank, a->ItemLevel, left) <
                         std::tuple(b->RequiredSkillRank, b->ItemLevel, right);
              });
    toolsInitialized = true;
}

std::uint32_t KnownRecipeSpell(Player* bot, std::uint32_t item)
{
    if (!toolsInitialized)
        throw std::runtime_error("ADVAI recipe index was not initialized");
    auto recipes = productRecipes.find(item);
    if (!bot || recipes == productRecipes.end())
        return 0;
    for (auto id : recipes->second)
        if (auto found = bot->GetSpellMap().find(id); found != bot->GetSpellMap().end())
            if (auto* known = found->second; known && known->State != PLAYERSPELL_REMOVED && known->Active &&
                known->IsInSpec(bot->GetActiveSpec()))
                return id;
    return 0;
}

std::vector<ObjectGuid> BankIngredients(Player* bot, RecipePlan const& recipe)
{
    std::vector<ObjectGuid> items;
    if (!bot || !recipe.MissingItem || !recipe.MissingCount)
        return items;
    std::uint32_t found = 0;
    auto consider = [&](Item* item)
    {
        if (item && item->GetEntry() == recipe.MissingItem && found < recipe.MissingCount)
        {
            items.push_back(item->GetGUID());
            found += item->GetCount();
        }
    };
    for (std::uint8_t slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        consider(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (std::uint8_t slot = BANK_SLOT_BAG_START; slot < BANK_SLOT_BAG_END; ++slot)
        if (auto* bag = bot->GetBagByPos(slot))
            for (std::uint32_t position = 0; position < bag->GetBagSize(); ++position)
                consider(bag->GetItemByPos(static_cast<std::uint8_t>(position)));
    return items;
}

bool HasCraftingStation(PlayerbotAI* ai, RecipePlan const& recipe)
{
    auto* spell = sSpellMgr->GetSpellInfo(recipe.Spell);
    if (!ai || !spell)
        return false;
    if (!spell->RequiresSpellFocus)
        return true;
    Acore::GameObjectFocusCheck eligible(ai->GetBot(), spell->RequiresSpellFocus);
    for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("nearest game objects no los")->Get())
        if (auto* object = ai->GetGameObject(guid); object && eligible(object) &&
            (object->GetPhaseMask() & ai->GetBot()->GetPhaseMask()))
            return true;
    return false;
}

std::optional<ServiceLocation> FindCraftingStation(Player* bot, RecipePlan const& recipe,
                                                   std::vector<ServiceLocation> const& excluded)
{
    auto* spell = sSpellMgr->GetSpellInfo(recipe.Spell);
    if (!bot || !spell || !spell->RequiresSpellFocus)
        return std::nullopt;
    auto found = craftStations.find(spell->RequiresSpellFocus);
    if (found == craftStations.end())
        return std::nullopt;
    std::optional<ServiceLocation> selected;
    float nearest = std::numeric_limits<float>::max();
    for (auto const& station : found->second)
    {
        auto const& location = station.Location;
        if (!(station.Phase & bot->GetPhaseMask()) || !HasTravelConnection(bot->GetMapId(), location.Map) ||
            std::any_of(excluded.begin(), excluded.end(), [&](auto const& blocked)
                { return blocked.Entry == location.Entry && blocked.Map == location.Map &&
                         blocked.X == location.X && blocked.Y == location.Y && blocked.Z == location.Z; }))
            continue;
        if (auto* faction = sFactionTemplateStore.LookupEntry(location.Faction);
            faction && bot->GetFactionTemplateEntry()->IsHostileTo(*faction))
            continue;
        float distance = location.Map == bot->GetMapId()
            ? bot->GetExactDist(location.X, location.Y, location.Z) : 20000.0f;
        if (distance < nearest)
        {
            nearest = distance;
            selected = location;
        }
    }
    return selected;
}

bool RecipeCompleted(Player* bot, RecipePlan const& recipe)
{
    if (!bot)
        return false;
    if (recipe.Enchantment)
    {
        auto* item = bot->GetItemByGuid(recipe.TargetItem);
        return item && item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) == recipe.Enchantment &&
               recipe.BeforeEnchantment != recipe.Enchantment;
    }
    return recipe.Product && bot->GetItemCount(recipe.Product, false) > recipe.Before;
}

bool RefreshRecipe(Player* bot, RecipePlan& recipe)
{
    SpellInfo const* spell = sSpellMgr->GetSpellInfo(recipe.Spell);
    if (!spell || !bot->HasSpell(recipe.Spell))
        return false;
    if (recipe.Enchantment)
    {
        auto* target = bot->GetItemByGuid(recipe.TargetItem);
        if (!target || !target->IsEquipped() || target->IsInTrade() ||
            !target->IsFitToSpellRequirements(spell) ||
            target->GetEnchantmentId(PERM_ENCHANTMENT_SLOT) != recipe.BeforeEnchantment)
            return false;
    }
    recipe.MissingItem = 0;
    recipe.MissingCount = 0;
    recipe.MissingTool = false;
    for (auto item : spell->Totem)
        if (item && !bot->GetItemCount(item, false))
        {
            recipe.MissingItem = item;
            recipe.MissingCount = 1;
            recipe.MissingTool = true;
            return true;
        }
    if (!toolsInitialized)
        throw std::runtime_error("ADVAI profession tool index was not initialized");
    for (auto category : spell->TotemCategory)
        if (category && !bot->HasItemTotemCategory(category))
        {
            for (auto id : professionTools)
                if (auto* item = sObjectMgr->GetItemTemplate(id);
                    item && bot->GetItemCount(id, true) && bot->IsTotemCategoryCompatiableWith(item, category))
                {
                    recipe.MissingItem = id;
                    recipe.MissingCount = 1;
                    recipe.MissingTool = true;
                    return true;
                }
            for (auto id : professionTools)
                if (auto* item = sObjectMgr->GetItemTemplate(id);
                    item && bot->IsTotemCategoryCompatiableWith(item, category) &&
                    bot->CanUseItem(item) == EQUIP_ERR_OK)
                {
                    recipe.MissingItem = id;
                    recipe.MissingCount = 1;
                    recipe.MissingTool = true;
                    return true;
                }
            return false;
        }
    for (std::size_t i = 0; i < MAX_SPELL_REAGENTS; ++i)
        if (spell->Reagent[i] > 0 && spell->ReagentCount[i])
        {
            auto held = bot->GetItemCount(spell->Reagent[i], false);
            if (held < spell->ReagentCount[i])
            {
                recipe.MissingItem = spell->Reagent[i];
                recipe.MissingCount = spell->ReagentCount[i] - held;
                break;
            }
        }
    return true;
}

std::optional<RecipePlan> RecipeForItem(Player* bot, std::uint32_t item)
{
    std::optional<RecipePlan> selected;
    auto recipes = productRecipes.find(item);
    if (!bot || recipes == productRecipes.end())
        return selected;
    for (auto id : recipes->second)
    {
        auto found = bot->GetSpellMap().find(id);
        if (found == bot->GetSpellMap().end())
            continue;
        auto* known = found->second;
        if (!known || known->State == PLAYERSPELL_REMOVED || !known->Active ||
            !known->IsInSpec(bot->GetActiveSpec()) || bot->HasSpellCooldown(id))
            continue;
        auto* spell = sSpellMgr->GetSpellInfo(id);
        if (!spell || spell->IsPassive() ||
            std::none_of(spell->Effects.begin(), spell->Effects.end(), [item](auto const& effect)
                         { return effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.ItemType == item; }))
            continue;
        RecipePlan recipe{id, item, bot->GetItemCount(item, false)};
        if (RefreshRecipe(bot, recipe) && (!selected || (!recipe.MissingItem && selected->MissingItem) ||
            (bool(recipe.MissingItem) == bool(selected->MissingItem) && id < selected->Spell)))
            selected = recipe;
    }
    return selected;
}

std::optional<RecipePlan> RestoreRecipe(PlayerbotAI* ai, std::uint32_t id)
{
    Player* bot = ai ? ai->GetBot() : nullptr;
    if (!bot || !bot->HasSpell(id))
        return std::nullopt;
    auto known = bot->GetSpellMap().find(id);
    if (known == bot->GetSpellMap().end() || !known->second || !known->second->Active ||
        known->second->State == PLAYERSPELL_REMOVED || !known->second->IsInSpec(bot->GetActiveSpec()))
        return std::nullopt;
    auto* spell = sSpellMgr->GetSpellInfo(id);
    if (!spell || spell->IsPassive())
        return std::nullopt;
    if (auto enchant = EnchantRecipe(bot, spell); enchant && RefreshRecipe(bot, *enchant))
        return enchant;
    for (auto const& effect : spell->Effects)
        if (effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.ItemType)
        {
            RecipePlan recipe{id, effect.ItemType, bot->GetItemCount(effect.ItemType, false)};
            recipe.ForMarket = ai->GetAiObjectContext()->GetValue<ItemUsage>(
                                   "item usage", std::to_string(effect.ItemType))->Get() == ITEM_USAGE_AH;
            if (RefreshRecipe(bot, recipe))
                return recipe;
        }
    return std::nullopt;
}

std::vector<std::uint32_t> RecipeItems(RecipePlan const& recipe)
{
    std::vector<std::uint32_t> result;
    if (auto* spell = sSpellMgr->GetSpellInfo(recipe.Spell))
    {
        for (auto id : spell->Reagent)
            if (id > 0)
                result.push_back(static_cast<std::uint32_t>(id));
        for (auto id : spell->Totem)
            if (id)
                result.push_back(id);
        if (recipe.MissingItem)
            result.push_back(recipe.MissingItem);
        if (recipe.Product)
            result.push_back(recipe.Product);
    }
    return result;
}

std::optional<RecipePlan> ChooseRecipe(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    std::optional<RecipePlan> best;
    std::int32_t bestScore = -1;
    for (auto const& [id, known] : bot->GetSpellMap())
    {
        if (known->State == PLAYERSPELL_REMOVED || !known->Active || !known->IsInSpec(bot->GetActiveSpec()) ||
            bot->HasSpellCooldown(id))
            continue;
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(id);
        if (!spell || spell->IsPassive())
            continue;
        constexpr std::array professions = {SKILL_ALCHEMY,     SKILL_BLACKSMITHING, SKILL_COOKING,
                                            SKILL_ENCHANTING,  SKILL_ENGINEERING,   SKILL_FIRST_AID,
                                            SKILL_INSCRIPTION, SKILL_JEWELCRAFTING, SKILL_LEATHERWORKING,
                                            SKILL_TAILORING,   SKILL_MINING};
        if (std::none_of(professions.begin(), professions.end(),
                         [&](auto skill) { return bot->HasSkill(skill) && spell->IsAbilityOfSkillType(skill); }))
            continue;
        if (auto enchant = EnchantRecipe(bot, spell); enchant && RefreshRecipe(bot, *enchant))
        {
            std::int32_t score = 650 + (ItemUsageValue::SpellGivesSkillUp(id, bot) ? 100 : 0) +
                                 (!enchant->MissingItem ? 100 : 0) + ((id ^ bot->GetGUID().GetCounter()) % 31);
            if (score > bestScore)
            {
                best = *enchant;
                bestScore = score;
            }
        }
        std::uint32_t product = 0;
        for (auto const& effect : spell->Effects)
            if (effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.ItemType)
                product = effect.ItemType;
        if (!product || std::none_of(std::begin(spell->ReagentCount), std::end(spell->ReagentCount),
                                     [](auto count) { return count > 0; }))
            continue;
        auto* productInfo = sObjectMgr->GetItemTemplate(product);
        if (!productInfo)
            continue;
        auto held = bot->GetItemCount(product, true);
        auto stockLimit = std::max(1U, productInfo->GetMaxStackSize());
        // A crafter stops when personal stock is sufficient; skill gains alone do not justify endless production.
        if (held >= stockLimit)
            continue;
        auto usage = ai->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(product))->Get();
        bool skillUp = ItemUsageValue::SpellGivesSkillUp(id, bot);
        std::int32_t score = skillUp && !held ? 300 : 0;
        if (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE)
            score += 700;
        else if (usage == ITEM_USAGE_USE || usage == ITEM_USAGE_AMMO || usage == ITEM_USAGE_SKILL)
            score += 450;
        else if (usage == ITEM_USAGE_AH)
        {
            auto now = static_cast<Timestamp>(time(nullptr));
            auto market = ReadMarket(bot->GetFaction(), product);
            if (!market.HasDemand(now) || now > market.Observed + 300 || !market.UnitPrice)
                continue;
            std::uint64_t materials = 0;
            bool valued = true;
            for (std::size_t reagent = 0; reagent < MAX_SPELL_REAGENTS; ++reagent)
                if (spell->Reagent[reagent] > 0 && spell->ReagentCount[reagent])
                {
                    auto price = ReadMarket(bot->GetFaction(), spell->Reagent[reagent]);
                    if (!price.UnitPrice || now > price.Observed + 300)
                    {
                        valued = false;
                        break;
                    }
                    materials += price.UnitPrice * spell->ReagentCount[reagent];
                }
            // Include the opportunity cost of owned reagents and leave room for auction cuts/deposits.
            if (!valued || market.UnitPrice * 80 / 100 <= materials)
                continue;
            score += 250 + static_cast<std::int32_t>(std::min<std::uint64_t>(market.SoldUnits, std::uint64_t{150}));
        }
        if (!score)
            continue;
        RecipePlan candidate{id, product, bot->GetItemCount(product, false)};
        candidate.ForMarket = usage == ITEM_USAGE_AH;
        if (!RefreshRecipe(bot, candidate))
            continue;
        if (!candidate.MissingItem)
            score += 100;
        // Stable tie-breaking prevents recipe oscillation without making every crafter identical.
        score += (id ^ bot->GetGUID().GetCounter()) % 31;
        if (score > bestScore)
        {
            best = candidate;
            bestScore = score;
        }
    }
    return best;
}

bool BuyIngredientOperation::Execute()
{
    auto& manager = Manager::Instance();
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    if (!bot || !bot->IsInWorld() || !manager.Applies(bot) || !manager.Active(_bot.GetCounter()) ||
        !bot->IsAlive() || bot->IsInCombat() || bot->GetTradeData() || !bot->GetSession())
        return false;
    auto planned = manager.PlannedGoal(_bot.GetCounter());
    if (!planned || *planned != Goal::Craft || !RefreshRecipe(bot, _recipe) || !_recipe.MissingItem)
        return false;
    auto budget = std::min(_budget, bot->GetMoney());
    if (_recipe.ForMarket)
    {
        auto now = static_cast<Timestamp>(time(nullptr));
        auto product = ReadMarket(bot->GetFaction(), _recipe.Product);
        auto* spell = sSpellMgr->GetSpellInfo(_recipe.Spell);
        if (!spell || !product.HasDemand(now) || now > product.Observed + 300 || !product.UnitPrice)
        {
            manager.Report(_bot.GetCounter(), {Goal::Craft, Outcome::Blocked,
                                               "craft market demand changed before ingredient purchase"});
            return false;
        }
        std::uint64_t otherMaterials = 0;
        for (std::size_t index = 0; index < MAX_SPELL_REAGENTS; ++index)
            if (spell->Reagent[index] > 0 && spell->ReagentCount[index])
            {
                auto id = static_cast<std::uint32_t>(spell->Reagent[index]);
                auto price = ReadMarket(bot->GetFaction(), id);
                if (!price.UnitPrice || now > price.Observed + 300)
                {
                    manager.Report(_bot.GetCounter(), {Goal::Craft, Outcome::Blocked,
                                                       "craft material valuation is unavailable or stale"});
                    return false;
                }
                auto units = spell->ReagentCount[index];
                if (id == _recipe.MissingItem)
                    units = std::min<std::uint32_t>(units, bot->GetItemCount(id, false));
                otherMaterials += price.UnitPrice * units;
            }
        auto proceeds = product.UnitPrice * 80 / 100;
        if (otherMaterials >= proceeds)
        {
            manager.Report(_bot.GetCounter(), {Goal::Craft, Outcome::Blocked,
                                               "remaining craft proceeds cannot cover material costs"});
            return false;
        }
        budget = static_cast<std::uint32_t>(std::min<std::uint64_t>(budget, proceeds - otherMaterials));
    }
    if (_recipe.Source == IngredientSource::Bank)
    {
        manager.Report(_bot.GetCounter(), {Goal::Craft, Outcome::Blocked, "bank ingredients require withdrawal"});
        return false;
    }
    auto* npc = bot->GetNPCIfCanInteractWith(
        _vendor, _recipe.Source == IngredientSource::Auction ? UNIT_NPC_FLAG_AUCTIONEER : UNIT_NPC_FLAG_VENDOR);
    if (!npc)
    {
        manager.Report(_bot.GetCounter(), {Goal::Craft, Outcome::Blocked, "ingredient supplier unavailable"});
        return false;
    }
    if (_recipe.Source == IngredientSource::Auction)
    {
        if (manager.Delivery(_bot.GetCounter()))
        {
            manager.Report(_bot.GetCounter(),
                           {Goal::Craft, Outcome::Blocked, "previous auction delivery is still pending"});
            return false;
        }
        auto* house = sAuctionMgr->GetAuctionsMap(npc->GetFaction());
        auto page = ReadMarketListings(npc->GetFaction(), _recipe.MissingItem);
        auto observedNow = static_cast<Timestamp>(time(nullptr));
        if (!page.Observed || observedNow > page.Observed + 300)
        {
            manager.Report(_bot.GetCounter(), {Goal::Craft, Outcome::Started,
                                               "waiting for the bounded native auction index to refresh"});
            return true;
        }
        std::uint32_t selected = 0, price = 0;
        double unitPrice = std::numeric_limits<double>::max();
        for (auto id : page.Auctions)
        {
            auto* listing = house->GetAuction(id);
            if (!listing || listing->owner == _bot || listing->item_template != _recipe.MissingItem ||
                !listing->itemCount || !listing->buyout || listing->buyout > budget ||
                listing->expire_time <= static_cast<time_t>(observedNow) ||
                sCharacterCache->GetCharacterAccountIdByGuid(listing->owner) == bot->GetSession()->GetAccountId())
                continue;
            auto unit = static_cast<double>(listing->buyout) / listing->itemCount;
            if (unit < unitPrice)
            {
                selected = id;
                price = listing->buyout;
                unitPrice = unit;
            }
        }
        if (!selected)
        {
            manager.Report(_bot.GetCounter(),
                           {Goal::Craft, page.Complete ? Outcome::Blocked : Outcome::Started,
                            page.Complete ? "ingredient has no affordable eligible indexed auction listing"
                                          : "auction ingredient search continues through the next bounded page"});
            return !page.Complete;
        }
        auto before = bot->GetMoney();
        auto* listing = house->GetAuction(selected);
        if (!listing)
            return false;
        auto itemGuid = listing->item_guid.GetCounter();
        auto now = static_cast<Timestamp>(time(nullptr));
        if (!manager.ExpectDelivery(_bot.GetCounter(),
                                    {itemGuid, _recipe.MissingItem, listing->itemCount, now, now + 24 * 3600}))
        {
            manager.Report(_bot.GetCounter(),
                           {Goal::Craft, Outcome::Blocked, "could not reserve expected auction delivery"});
            return false;
        }
        WorldPacket packet(CMSG_AUCTION_PLACE_BID);
        packet << _vendor << selected << price;
        bot->GetSession()->HandleAuctionPlaceBid(packet);
        bool purchased = bot->GetMoney() < before && !house->GetAuction(selected);
        if (!purchased)
            manager.FinishDelivery(_bot.GetCounter(), itemGuid, false);
        manager.Report(_bot.GetCounter(),
                       {Goal::Craft, purchased ? Outcome::Started : Outcome::Blocked,
                        purchased ? "ingredient purchased; collect actual auction delivery from mailbox"
                                  : "auction purchase rejected"});
        return purchased;
    }
    auto* stock = npc->GetVendorItems();
    auto* item = sObjectMgr->GetItemTemplate(_recipe.MissingItem);
    if (!stock || !item)
        return false;
    for (std::uint32_t slot = 0; slot < stock->GetItemCount(); ++slot)
    {
        auto* offered = stock->GetItem(slot);
        if (!offered || offered->item != _recipe.MissingItem || offered->ExtendedCost)
            continue;
        auto before = bot->GetItemCount(item->ItemId, false);
        std::uint64_t cost = static_cast<std::uint64_t>(item->BuyPrice * bot->GetReputationPriceDiscount(npc));
        std::uint32_t batches =
            (_recipe.MissingCount + std::max(1U, item->BuyCount) - 1) / std::max(1U, item->BuyCount);
        if (cost * batches > budget)
        {
            manager.Report(_bot.GetCounter(),
                           {Goal::Craft, Outcome::Blocked, "ingredient vendor cost exceeds craft budget"});
            return false;
        }
        bot->BuyItemFromVendorSlot(_vendor, slot, item->ItemId, batches, NULL_BAG, NULL_SLOT);
        bool purchased = bot->GetItemCount(item->ItemId, false) > before;
        manager.Report(_bot.GetCounter(),
                       {Goal::Craft, purchased ? Outcome::Started : Outcome::Blocked,
                        purchased ? "recipe ingredient purchased from vendor" : "vendor rejected ingredient purchase"});
        return purchased;
    }
    manager.Report(_bot.GetCounter(), {Goal::Craft, Outcome::Blocked, "supplier no longer stocks required ingredient"});
    return false;
}
}  // namespace Advai
