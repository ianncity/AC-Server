/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiSupplies.h"

#include <algorithm>
#include <map>

#include "AdvaiMgr.h"
#include "Bag.h"
#include "Creature.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpellRepository.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace Advai
{
namespace
{
struct ProfessionTool
{
    std::uint32_t Skill;
    std::uint32_t Category;
    std::uint32_t Item;
};
constexpr ProfessionTool professionTools[] = {{SKILL_MINING, TC_MINING_PICK, 2901},
                                              {SKILL_SKINNING, TC_SKINNING_KNIFE, 7005},
                                              {SKILL_BLACKSMITHING, TC_BLACKSMITH_HAMMER, 5956},
                                              {SKILL_ENGINEERING, TC_ARCLIGHT_SPANNER, 6219},
                                              {SKILL_ENGINEERING, TC_BLACKSMITH_HAMMER, 5956}};

SpellInfo const* MountSpell(ItemTemplate const* item)
{
    if (!item)
        return nullptr;
    for (auto const& entry : item->Spells)
        if (entry.SpellId > 0)
            if (auto* spell = sSpellMgr->GetSpellInfo(entry.SpellId))
            {
                if (spell->HasAura(SPELL_AURA_MOUNTED))
                    return spell;
                for (auto const& effect : spell->Effects)
                    if (effect.Effect == SPELL_EFFECT_LEARN_SPELL)
                        if (auto* learned = sSpellMgr->GetSpellInfo(effect.TriggerSpell);
                            learned && learned->HasAura(SPELL_AURA_MOUNTED))
                            return learned;
            }
    return nullptr;
}

std::uint32_t MountSpeed(Player* bot, SpellInfo const* spell, bool flying)
{
    std::uint32_t speed = 0;
    auto aura = flying ? SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED : SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED;
    for (auto const& effect : spell->Effects)
        if (effect.ApplyAuraName == aura || (flying && effect.ApplyAuraName == SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED))
            speed = std::max(speed, static_cast<std::uint32_t>(std::max(0, effect.CalcValue(bot))));
    return speed;
}

bool WantedMount(Player* bot, ItemTemplate const* item, bool flying, std::uint32_t speed)
{
    auto* spell = MountSpell(item);
    return spell && !bot->HasSpell(spell->Id) && bot->CanUseItem(item) == EQUIP_ERR_OK &&
           PlayerbotSpellRepository::Instance().GetRequiredRidingSkill(spell) <= bot->GetPureSkillValue(SKILL_RIDING) &&
           spell->HasAura(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) == flying &&
           MountSpeed(bot, spell, flying) >= speed;
}

bool IsPetFood(Player* bot, ItemTemplate const* item)
{
    Pet* pet = bot->GetPet();
    return pet && pet->getPetType() == HUNTER_PET && pet->HaveInDiet(item) &&
           pet->GetCurrentFoodBenefitLevel(item->ItemLevel) > 0;
}

bool UsableAmmo(Player* bot, ItemTemplate const* item)
{
    Item* ranged = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!ranged || item->Class != ITEM_CLASS_PROJECTILE || bot->CanUseItem(item) != EQUIP_ERR_OK)
        return false;
    auto weapon = ranged->GetTemplate()->SubClass;
    return (weapon == ITEM_SUBCLASS_WEAPON_GUN && item->SubClass == ITEM_SUBCLASS_BULLET) ||
           ((weapon == ITEM_SUBCLASS_WEAPON_BOW || weapon == ITEM_SUBCLASS_WEAPON_CROSSBOW) &&
            item->SubClass == ITEM_SUBCLASS_ARROW);
}

std::string FoodType(Player* bot, ItemTemplate const* item, bool usesMana)
{
    if (item->Class != ITEM_CLASS_CONSUMABLE || bot->CanUseItem(item) != EQUIP_ERR_OK ||
        item->ItemLevel + 15 < bot->GetLevel())
        return {};
    return ItemUsageValue::GetConsumableType(item, usesMana);
}
}  // namespace

std::vector<ReagentNeed> ClassReagents(Player const* bot)
{
    std::map<std::uint32_t, std::uint32_t> targets;
    for (auto const& [id, known] : bot->GetSpellMap())
    {
        if (!known || known->State == PLAYERSPELL_REMOVED || !known->Active || !known->IsInSpec(bot->GetActiveSpec()))
            continue;
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(id);
        if (!spell || spell->IsPassive() || spell->IsProfessionOrRiding() ||
            spell->SpellFamilyName == SPELLFAMILY_GENERIC || bot->CanNoReagentCast(spell))
            continue;
        for (std::uint8_t slot = 0; slot < MAX_SPELL_REAGENTS; ++slot)
            if (spell->Reagent[slot] > 0 && spell->ReagentCount[slot] > 0)
            {
                auto item = static_cast<std::uint32_t>(spell->Reagent[slot]);
                auto count = static_cast<std::uint32_t>(spell->ReagentCount[slot]);
                // Keep five casts, including spells that consume more than one item per cast.
                targets[item] = std::max(targets[item], count * 5);
            }
    }
    std::vector<ReagentNeed> result;
    for (auto [item, count] : targets)
        result.push_back({item, count});
    return result;
}

bool WantedRecipe(Player* bot, ItemTemplate const* item)
{
    if (!item || item->Class != ITEM_CLASS_RECIPE || !item->RequiredSkill ||
        !bot->HasSkill(item->RequiredSkill) ||
        bot->GetPureSkillValue(item->RequiredSkill) < item->RequiredSkillRank ||
        bot->CanUseItem(item) != EQUIP_ERR_OK)
        return false;
    for (auto const& entry : item->Spells)
        if (entry.SpellId > 0)
        {
            // Recipe items use either the generic learn wrapper (483/55884) plus a recipe
            // spell, or a dedicated spell whose effect teaches the recipe.
            if (entry.SpellTrigger == ITEM_SPELLTRIGGER_LEARN_SPELL_ID && !bot->HasSpell(entry.SpellId))
                return true;
            if (auto* spell = sSpellMgr->GetSpellInfo(entry.SpellId))
                for (auto const& effect : spell->Effects)
                    if (effect.Effect == SPELL_EFFECT_LEARN_SPELL && effect.TriggerSpell &&
                        !bot->HasSpell(effect.TriggerSpell))
                        return true;
        }
    return false;
}

SupplyPlan CaptureSupplies(PlayerbotAI* ai, std::vector<ReagentNeed> const& reagents)
{
    Player* bot = ai->GetBot();
    SupplyPlan plan;
    plan.Reagents = reagents;
    plan.UsesMana = bot->GetMaxPower(POWER_MANA) > 0;
    if (auto* weapon = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED))
    {
        auto subclass = weapon->GetTemplate()->SubClass;
        plan.UsesAmmo = weapon->GetTemplate()->Class == ITEM_CLASS_WEAPON &&
                        (subclass == ITEM_SUBCLASS_WEAPON_GUN || subclass == ITEM_SUBCLASS_WEAPON_BOW ||
                         subclass == ITEM_SUBCLASS_WEAPON_CROSSBOW);
    }
    plan.HasHungryPet = bot->GetPet() && bot->GetPet()->getPetType() == HUNTER_PET;
    std::uint32_t groundSpeed = 0, flyingSpeed = 0;
    auto riding = bot->GetPureSkillValue(SKILL_RIDING);
    for (auto const& [id, learned] : bot->GetSpellMap())
        if (learned && learned->State != PLAYERSPELL_REMOVED && learned->Active &&
            learned->IsInSpec(bot->GetActiveSpec()))
            if (auto* spell = sSpellMgr->GetSpellInfo(id);
                spell && ((spell->HasAura(SPELL_AURA_MOUNTED) &&
                    PlayerbotSpellRepository::Instance().GetRequiredRidingSkill(spell) <= riding) ||
                    (spell->HasAura(SPELL_AURA_MOD_SHAPESHIFT) &&
                     spell->HasAura(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED))))
            {
                if (spell->HasAura(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) ||
                    spell->HasAura(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED))
                    flyingSpeed = std::max(flyingSpeed, MountSpeed(bot, spell, true));
                else
                    groundSpeed = std::max(groundSpeed, MountSpeed(bot, spell, false));
            }
    auto desiredGround = riding >= 150 ? 100U : 60U;
    auto desiredFlight = riding >= 300 ? 280U : 60U;
    bool flightRegion = bot->GetMapId() == 530 || (bot->GetMapId() == 571 && bot->HasSpell(54197));
    bool needsGround = riding >= 75 && groundSpeed < desiredGround;
    plan.WantsFlyingMount = !needsGround && riding >= 225 && flightRegion && flyingSpeed < desiredFlight;
    plan.NeedsMount = needsGround || plan.WantsFlyingMount;
    plan.RequiredMountSpeed = plan.WantsFlyingMount ? desiredFlight : desiredGround;
    bool fishingPole = false;
    for (Item* item : ai->GetInventoryAndEquippedItems())
    {
        if (!item)
            continue;
        ItemTemplate const* proto = item->GetTemplate();
        if (!item->IsEquipped() && WantedRecipe(bot, proto))
            plan.RecipeItem = item->GetEntry();
        if (plan.NeedsMount && WantedMount(bot, proto, plan.WantsFlyingMount, plan.RequiredMountSpeed))
            plan.MountItem = item->GetEntry();
        fishingPole =
            fishingPole || (proto->Class == ITEM_CLASS_WEAPON && proto->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE);
        if (item->IsEquipped())
            continue;
        auto food = FoodType(bot, proto, plan.UsesMana);
        if (food == "food")
            plan.Food += item->GetCount();
        if (food == "drink")
            plan.Drink += item->GetCount();
        if (plan.UsesAmmo && UsableAmmo(bot, proto))
            plan.Ammo += item->GetCount();
        if (plan.HasHungryPet && IsPetFood(bot, proto))
            plan.PetFood += item->GetCount();
    }
    for (auto const& tool : professionTools)
        if (bot->HasSkill(tool.Skill) && !bot->HasItemTotemCategory(tool.Category) && !bot->GetItemCount(tool.Item))
            plan.Tools.push_back({tool.Item, 1});
    if (bot->HasSkill(SKILL_FISHING) && !fishingPole)
        plan.Tools.push_back({6256, 1});
    plan.BankSupply = FindBankSupply(bot, plan);
    return plan;
}

ObjectGuid FindBankSupply(Player* bot, SupplyPlan const& plan)
{
    // One native stack transfer at a time; refresh actual carried totals before
    // choosing another so equivalent food/reagent stacks are not all withdrawn.
    auto useful = [&](Item* item)
    {
        return item && !item->IsInTrade() && plan.Missing(bot, item->GetTemplate());
    };
    for (std::uint8_t slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        if (auto* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot); useful(item))
            return item->GetGUID();
    for (std::uint8_t slot = BANK_SLOT_BAG_START; slot < BANK_SLOT_BAG_END; ++slot)
        if (auto* bag = bot->GetBagByPos(slot))
            for (std::uint32_t position = 0; position < bag->GetBagSize(); ++position)
                if (auto* item = bag->GetItemByPos(static_cast<std::uint8_t>(position)); useful(item))
                    return item->GetGUID();
    return ObjectGuid::Empty;
}

bool SupplyPlan::NeedsReagents(Player const* bot) const
{
    return std::any_of(Reagents.begin(), Reagents.end(),
                       [&](auto const& need) { return bot->GetItemCount(need.Item, false) < need.Target; });
}

std::uint32_t SupplyPlan::Missing(Player* bot, ItemTemplate const* item) const
{
    std::uint32_t missing = 0;
    bool essentials = Food >= 3 && (!UsesMana || Drink >= 3) && (!UsesAmmo || Ammo >= 100) &&
                      (!HasHungryPet || PetFood >= 3) && !NeedsReagents(bot) &&
                      std::all_of(Tools.begin(), Tools.end(),
                                  [&](auto const& tool) { return bot->GetItemCount(tool.Item, false) >= tool.Target; });
    if (NeedsMount && !MountItem && essentials && WantedMount(bot, item, WantsFlyingMount, RequiredMountSpeed))
        missing = 1;
    for (auto const& tool : Tools)
        if (tool.Item == item->ItemId && !bot->GetItemCount(tool.Item))
            missing = 1;
    for (auto const& reagent : Reagents)
        if (reagent.Item == item->ItemId)
        {
            auto count = bot->GetItemCount(reagent.Item, false);
            if (count < reagent.Target)
                missing = reagent.Target - count;
        }
    auto food = FoodType(bot, item, UsesMana);
    if (food == "food" && Food < 3)
        missing = std::max(missing, 10 - Food);
    if (food == "drink" && UsesMana && Drink < 3)
        missing = std::max(missing, 10 - Drink);
    if (UsesAmmo && Ammo < 100 && UsableAmmo(bot, item))
        missing = std::max(missing, 1000 - Ammo);
    if (HasHungryPet && PetFood < 3 && IsPetFood(bot, item))
        missing = std::max(missing, 10 - PetFood);
    return missing;
}

bool IsSupplyForUse(Player* bot, ItemTemplate const* item, SupplyPlan const& plan)
{
    if (WantedRecipe(bot, item))
        return true;
    if (plan.NeedsMount && WantedMount(bot, item, plan.WantsFlyingMount, plan.RequiredMountSpeed))
        return true;
    // Preserve tools for learned recipes, including advanced rods and compatible multi-tools.
    if (item->TotemCategory)
        for (auto const& [id, known] : bot->GetSpellMap())
        {
            if (!known || known->State == PLAYERSPELL_REMOVED || !known->Active)
                continue;
            if (auto* spell = sSpellMgr->GetSpellInfo(id))
                for (auto category : spell->TotemCategory)
                    if (category && bot->IsTotemCategoryCompatiableWith(item, category))
                        return true;
        }
    if (bot->HasSkill(SKILL_FISHING) && item->Class == ITEM_CLASS_WEAPON &&
        item->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
        return true;
    for (auto const& tool : professionTools)
        if (bot->HasSkill(tool.Skill) &&
            (tool.Item == item->ItemId || bot->IsTotemCategoryCompatiableWith(item, tool.Category)))
            return true;
    return std::any_of(
               plan.Reagents.begin(), plan.Reagents.end(), [&](auto const& reagent)
               { return reagent.Item == item->ItemId && bot->GetItemCount(reagent.Item) <= reagent.Target * 2; }) ||
           (plan.HasHungryPet && plan.PetFood < 20 && IsPetFood(bot, item));
}

std::shared_ptr<SupplyPlan const> Manager::Supplies(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->Supplies;
    }
    return {};
}

bool StocksNeededSupplies(Player* bot, std::uint32_t entry, SupplyPlan const& plan)
{
    VendorItemData const* stock = sObjectMgr->GetNpcVendorItemList(entry);
    if (!stock)
        return false;
    for (VendorItem const* offered : stock->m_items)
        if (offered && !offered->ExtendedCost)
            if (auto* item = sObjectMgr->GetItemTemplate(offered->item);
                item && item->BuyPrice && plan.Missing(bot, item))
                return true;
    return false;
}

bool BuySuppliesOperation::Execute()
{
    Player* bot = ObjectAccessor::FindPlayer(_bot);
    auto& manager = Manager::Instance();
    if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat() || !manager.Applies(bot) ||
        !manager.Active(_bot.GetCounter()))
        return false;
    auto* npc = bot->GetNPCIfCanInteractWith(_vendor, UNIT_NPC_FLAG_VENDOR);
    auto* ai = GET_PLAYERBOT_AI(bot);
    if (!npc || !ai || !npc->GetVendorItems())
    {
        manager.Report(_bot.GetCounter(), {Goal::Restock, Outcome::Blocked, "supply vendor unavailable"});
        return false;
    }
    // Refresh both inventory and glyph exemptions at execution; queued plans do not grant items.
    auto reagents = ClassReagents(bot);
    auto plan = CaptureSupplies(ai, reagents);
    auto* stock = npc->GetVendorItems();
    bool purchased = false;
    for (std::uint32_t slot = 0; slot < stock->GetItemCount(); ++slot)
    {
        auto* offered = stock->GetItem(slot);
        if (!offered || offered->ExtendedCost)
            continue;
        auto* item = sObjectMgr->GetItemTemplate(offered->item);
        if (!item || !item->BuyPrice)
            continue;
        bool recipe = !purchased && WantedRecipe(bot, item) && !bot->GetItemCount(item->ItemId, true);
        auto missing = recipe ? 1U : plan.Missing(bot, item);
        if (!missing)
            continue;
        auto cost = static_cast<std::uint64_t>(item->BuyPrice * bot->GetReputationPriceDiscount(npc));
        cost = std::max<std::uint64_t>(1, cost);
        if (recipe)
        {
            auto persona = manager.Persona(_bot.GetCounter());
            auto reserve = static_cast<std::uint64_t>(bot->GetLevel()) * bot->GetLevel() *
                           (10 + persona.Frugality / 5);
            if (cost + reserve > bot->GetMoney())
                continue;
        }
        if (plan.NeedsMount && WantedMount(bot, item, plan.WantsFlyingMount, plan.RequiredMountSpeed))
        {
            auto persona = manager.Persona(_bot.GetCounter());
            std::uint64_t reserve = static_cast<std::uint64_t>(bot->GetLevel()) * bot->GetLevel() *
                                    (10 + persona.Frugality / 5);
            if (cost + reserve > bot->GetMoney())
                continue;
        }
        auto batches = (missing + std::max(1U, item->BuyCount) - 1) / std::max(1U, item->BuyCount);
        batches = static_cast<std::uint32_t>(std::min<std::uint64_t>({batches, 255, bot->GetMoney() / cost}));
        if (!batches)
            continue;
        auto before = bot->GetItemCount(item->ItemId, false);
        bot->BuyItemFromVendorSlot(_vendor, slot, item->ItemId, static_cast<std::uint8_t>(batches), NULL_BAG,
                                   NULL_SLOT);
        if (bot->GetItemCount(item->ItemId, false) > before)
        {
            purchased = true;
            plan = CaptureSupplies(ai, reagents);
        }
    }
    manager.Report(_bot.GetCounter(),
                   {Goal::Restock, purchased ? Outcome::Started : Outcome::Blocked,
                    purchased ? "needed supplies purchased through vendor checks"
                              : "supply purchase blocked by funds, stock, requirements or bag space"});
    return purchased;
}
}  // namespace Advai
