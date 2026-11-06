/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_PROFESSION_H
#define PLAYERBOTS_ADVAI_PROFESSION_H

#include "AdvaiModel.h"
#include "PlayerbotOperation.h"

class Player;
class PlayerbotAI;

namespace Advai
{
enum class IngredientSource : std::uint8_t
{
    Vendor,
    Auction,
    Bank
};

struct RecipePlan
{
    std::uint32_t Spell = 0;
    std::uint32_t Product = 0;
    std::uint32_t Before = 0;
    std::uint32_t MissingItem = 0;
    std::uint32_t MissingCount = 0;
    IngredientSource Source = IngredientSource::Vendor;
    bool ForMarket = false;
    bool MissingTool = false;
    ObjectGuid TargetItem;
    std::uint32_t Enchantment = 0;
    std::uint32_t BeforeEnchantment = 0;
};

void InitializeProfessionTools();
std::uint32_t KnownRecipeSpell(Player* bot, std::uint32_t item);
std::optional<RecipePlan> ChooseRecipe(PlayerbotAI* ai);
std::optional<RecipePlan> RecipeForItem(Player* bot, std::uint32_t item);
std::optional<RecipePlan> RestoreRecipe(PlayerbotAI* ai, std::uint32_t spell);
std::vector<std::uint32_t> RecipeItems(RecipePlan const& recipe);
bool RefreshRecipe(Player* bot, RecipePlan& recipe);
bool RecipeCompleted(Player* bot, RecipePlan const& recipe);
bool HasCraftingStation(PlayerbotAI* ai, RecipePlan const& recipe);
std::optional<ServiceLocation> FindCraftingStation(Player* bot, RecipePlan const& recipe,
                                                   std::vector<ServiceLocation> const& excluded);
std::vector<ObjectGuid> BankIngredients(Player* bot, RecipePlan const& recipe);

class BuyIngredientOperation : public PlayerbotOperation
{
public:
    BuyIngredientOperation(ObjectGuid bot, ObjectGuid vendor, RecipePlan recipe, std::uint32_t budget)
        : _bot(bot), _vendor(vendor), _recipe(recipe), _budget(budget)
    {
    }
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI acquire recipe ingredient"; }

private:
    ObjectGuid _bot;
    ObjectGuid _vendor;
    RecipePlan _recipe;
    std::uint32_t _budget;
};
}  // namespace Advai

#endif
