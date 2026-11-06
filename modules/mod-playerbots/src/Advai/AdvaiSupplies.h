/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_SUPPLIES_H
#define PLAYERBOTS_ADVAI_SUPPLIES_H

#include <cstdint>
#include <vector>

#include "PlayerbotOperation.h"

struct ItemTemplate;
class Player;
class PlayerbotAI;

namespace Advai
{
struct ReagentNeed
{
    std::uint32_t Item = 0;
    std::uint32_t Target = 0;
};

// Published per-bot facts contain identifiers and quantities, never live object pointers.
struct SupplyPlan
{
    std::vector<ReagentNeed> Reagents;
    std::vector<ReagentNeed> Tools;
    std::uint32_t Food = 0;
    std::uint32_t Drink = 0;
    std::uint32_t Ammo = 0;
    std::uint32_t PetFood = 0;
    bool UsesMana = false;
    bool UsesAmmo = false;
    bool HasHungryPet = false;
    bool NeedsMount = false;
    bool WantsFlyingMount = false;
    std::uint32_t MountItem = 0;
    std::uint32_t RequiredMountSpeed = 0;
    ObjectGuid BankSupply;
    std::uint32_t RecipeItem = 0;

    bool NeedsReagents(Player const* bot) const;
    std::uint32_t Missing(Player* bot, ItemTemplate const* item) const;
};

std::vector<ReagentNeed> ClassReagents(Player const* bot);
SupplyPlan CaptureSupplies(PlayerbotAI* ai, std::vector<ReagentNeed> const& reagents);
bool StocksNeededSupplies(Player* bot, std::uint32_t entry, SupplyPlan const& plan);
bool IsSupplyForUse(Player* bot, ItemTemplate const* item, SupplyPlan const& plan);
bool WantedRecipe(Player* bot, ItemTemplate const* item);
ObjectGuid FindBankSupply(Player* bot, SupplyPlan const& plan);

class BuySuppliesOperation : public PlayerbotOperation
{
public:
    BuySuppliesOperation(ObjectGuid bot, ObjectGuid vendor) : _bot(bot), _vendor(vendor) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI buy needed supplies"; }

private:
    ObjectGuid _bot;
    ObjectGuid _vendor;
};
}  // namespace Advai

#endif
