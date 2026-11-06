/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiKnowledge.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>

#include "DBCStores.h"
#include "AdvaiAscensionProgression.h"
#include "AdvaiProfession.h"
#include "AdvaiMgr.h"
#include "AiFactory.h"
#include "ArenaTeam.h"
#include "ArenaTeamMgr.h"
#include "Battleground.h"
#include "PlayerbotAI.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "World.h"
#include "Item.h"
#include "SpellMgr.h"
#include "SpellInfo.h"

namespace Advai
{
namespace
{
struct Profession { std::uint32_t Id; char const* Token; char const* Name; bool Primary; };
static constexpr Profession skills[] = {
        {SKILL_ALCHEMY, "alchemy", "Alchemy", true}, {SKILL_BLACKSMITHING, "blacksmithing", "Blacksmithing", true},
        {SKILL_ENCHANTING, "enchanting", "Enchanting", true}, {SKILL_ENGINEERING, "engineering", "Engineering", true},
        {SKILL_HERBALISM, "herbalism", "Herbalism", true}, {SKILL_INSCRIPTION, "inscription", "Inscription", true},
        {SKILL_JEWELCRAFTING, "jewelcrafting", "Jewelcrafting", true},
        {SKILL_LEATHERWORKING, "leatherworking", "Leatherworking", true}, {SKILL_MINING, "mining", "Mining", true},
        {SKILL_SKINNING, "skinning", "Skinning", true}, {SKILL_TAILORING, "tailoring", "Tailoring", true},
        {SKILL_COOKING, "cooking", "Cooking", false}, {SKILL_FISHING, "fishing", "Fishing", false},
        {SKILL_FIRST_AID, "firstaid", "First Aid", false}, {SKILL_RIDING, "riding", "Riding", false}};

bool AboutSender(ParsedMessage const& question)
{
    static std::unordered_set<std::string> const subjects = {
        "gear", "equipment", "weapon", "armor", "build", "spec", "talents", "class", "level", "profession",
        "professions", "mining", "herbalism", "alchemy", "blacksmithing", "tailoring", "leatherworking",
        "engineering", "enchanting", "jewelcrafting", "inscription", "cooking", "fishing", "riding", "firstaid"};
    for (std::size_t index = 1; index < question.Tokens.size(); ++index)
        if (question.Tokens[index - 1] == "my" && subjects.contains(question.Tokens[index]))
            return true;
    return false;
}

std::string Label(std::string const& name)
{
    constexpr std::size_t MAX_LABEL_BYTES = 60;
    if (name.size() <= MAX_LABEL_BYTES)
        return name;
    auto size = MAX_LABEL_BYTES;
    while (size && (static_cast<unsigned char>(name[size]) & 0xc0) == 0x80)
        --size;
    return name.substr(0, size) + "...";
}
}  // namespace

std::string SpecializationName(Player* bot)
{
    if (!bot)
        throw std::invalid_argument("ADVAI specialization facts require a player");
    std::string role = PlayerbotAI::IsTank(bot) ? "tank" : PlayerbotAI::IsHeal(bot) ? "healer" : "damage";
    if (bot->GetLevel() < 10)
        return "no specialization yet; " + role;
    if (auto spec = Ascension::Specialization(bot))
    {
        switch (spec)
        {
            case 34: return "Death (" + role + ')';
            case 35: return "Animation (" + role + ')';
            case 36: return "Rime (" + role + ')';
            case 40: return "Influence (" + role + ')';
            case 41: return "Corruption (" + role + ')';
            case 42: return "Godblade (" + role + ')';
            case 96: return "Bulwark (" + role + ')';
            default: throw std::runtime_error("ADVAI class dialogue references an unknown Ascension specialization");
        }
    }
    if (Manager::Instance().Profile(bot->GetGUID().GetCounter()) != ClassProfile::Classic)
        return role;
    static constexpr char const* names[][3] = {
        {"", "", ""}, {"Arms", "Fury", "Protection"}, {"Holy", "Protection", "Retribution"},
        {"Beast Mastery", "Marksmanship", "Survival"}, {"Assassination", "Combat", "Subtlety"},
        {"Discipline", "Holy", "Shadow"}, {"Blood", "Frost", "Unholy"},
        {"Elemental", "Enhancement", "Restoration"}, {"Arcane", "Fire", "Frost"},
        {"Affliction", "Demonology", "Destruction"}, {"", "", ""}, {"Balance", "Feral", "Restoration"}};
    auto spec = AiFactory::GetPlayerSpecTab(bot);
    auto cls = bot->getClass();
    if (cls >= std::size(names) || spec >= 3 || !names[cls][spec][0])
        throw std::runtime_error("ADVAI class dialogue has no native specialization definition");
    return std::string(names[cls][spec]) + " (" + role + ')';
}

std::uint32_t ProfessionSubject(ParsedMessage const& question)
{
    std::uint32_t selected = 0;
    for (auto const& skill : skills)
        if (std::find(question.Tokens.begin(), question.Tokens.end(), skill.Token) != question.Tokens.end() ||
            (skill.Id == SKILL_FIRST_AID && question.Normalized.find("first aid") != std::string::npos))
        {
            if (selected)
                return 0;
            selected = skill.Id;
        }
    return selected;
}

std::string ProfessionFacts(CatalogHandle const& catalog, std::uint32_t locale, Player* bot,
                            ParsedMessage const& question, std::uint32_t focus, std::uint32_t craftSpell)
{
    if (question.Question && AboutSender(question))
        return CatalogFact(catalog, "profession_other_unknown", locale, {});
    auto has = [&](char const* token)
    { return std::find(question.Tokens.begin(), question.Tokens.end(), token) != question.Tokens.end(); };
    bool projectQuestion = question.Question &&
        (has("crafting") || has("making") || has("ingredients") || has("materials") || has("reagents"));
    if (projectQuestion && !ProfessionSubject(question))
    {
        auto* spell = craftSpell ? sSpellMgr->GetSpellInfo(craftSpell) : nullptr;
        if (!spell || !bot->HasSpell(craftSpell))
            return CatalogFact(catalog, "craft_project_none", locale, {});
        RecipePlan recipe;
        recipe.Spell = craftSpell;
        if (!RefreshRecipe(bot, recipe))
            return CatalogFact(catalog, "craft_project_recheck", locale, {Label(spell->SpellName[0])});
        std::string result = CatalogFact(catalog, "craft_project", locale, {Label(spell->SpellName[0])});
        if (auto* item = sObjectMgr->GetItemTemplate(recipe.MissingItem))
            result += ' ' + CatalogFact(catalog, recipe.MissingTool ? "craft_project_tool" : "craft_project_material",
                                        locale, {Label(item->Name1), std::to_string(recipe.MissingCount)});
        else
            result += ' ' + CatalogFact(catalog, "craft_project_ready", locale, {});
        return result;
    }
    std::vector<Profession const*> selected;
    for (auto const& skill : skills)
        if (std::find(question.Tokens.begin(), question.Tokens.end(), skill.Token) != question.Tokens.end() ||
            (skill.Id == SKILL_FIRST_AID && question.Normalized.find("first aid") != std::string::npos))
            selected.push_back(&skill);
    if (selected.empty() && focus)
    {
        auto found = std::find_if(std::begin(skills), std::end(skills),
                                  [focus](auto const& skill) { return skill.Id == focus; });
        if (found == std::end(skills))
            throw std::runtime_error("ADVAI conversation references an unknown profession");
        selected.push_back(found);
    }
    if (selected.empty())
        for (auto const& skill : skills)
            if (skill.Primary && bot->HasSkill(skill.Id))
                selected.push_back(&skill);
    if (selected.empty())
        return CatalogFact(catalog, "profession_none", locale, {});
    if (selected.size() > 3)
        return CatalogFact(catalog, "profession_clarify", locale, {});
    std::string result;
    for (auto* skill : selected)
    {
        if (!result.empty())
            result += ' ';
        result += bot->HasSkill(skill->Id)
                      ? CatalogFact(catalog, "profession_skill", locale,
                                    {skill->Name, std::to_string(bot->GetPureSkillValue(skill->Id)),
                                     std::to_string(bot->GetPureMaxSkillValue(skill->Id))})
                      : CatalogFact(catalog, "profession_unlearned", locale, {skill->Name});
    }
    return result;
}

std::optional<std::string> EntityFacts(CatalogHandle const& catalog, std::uint32_t locale, Player* bot,
                                     Intent topic, std::uint32_t entity, ParsedMessage const* question)
{
    if (!bot || !entity)
        return std::nullopt;
    if (topic == Intent::ItemKnown)
    {
        auto* item = sObjectMgr->GetItemTemplate(entity);
        if (!item)
            return std::nullopt;
        if (question && std::any_of(question->Tokens.begin(), question->Tokens.end(), [](auto const& word)
                                   { return word == "craft" || word == "make" || word == "recipe"; }))
            return CatalogFact(catalog, KnownRecipeSpell(bot, entity) ? "recipe_known" : "recipe_unknown", locale,
                               {Label(item->Name1)});
        auto carried = bot->GetItemCount(entity, false);
        auto total = bot->GetItemCount(entity, true);
        bool equipped = false;
        for (std::uint8_t slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (auto* worn = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot); worn && worn->GetEntry() == entity)
                equipped = true;
        if (equipped)
            return CatalogFact(catalog, "item_equipped", locale, {Label(item->Name1), std::to_string(carried)});
        if (total)
            return CatalogFact(catalog, "item_stock", locale,
                               {Label(item->Name1), std::to_string(carried), std::to_string(total - carried)});
        return CatalogFact(catalog, "item_absent", locale, {Label(item->Name1)});
    }
    if (topic == Intent::SpellKnown)
    {
        auto* spell = sSpellMgr->GetSpellInfo(entity);
        if (!spell)
            return std::nullopt;
        auto key = !bot->HasSpell(entity) ? "spell_unknown"
                   : spell->IsPassive() ? "spell_passive"
                   : bot->HasSpellCooldown(entity) ? "spell_cooldown"
                                                  : "spell_learned";
        return CatalogFact(catalog, key, locale, {Label(spell->SpellName[0])});
    }
    return std::nullopt;
}

std::string LocalizedFacts(CatalogHandle const& catalog, std::uint32_t locale, Intent intent,
                           BotActivitySnapshot const& snapshot, BotLifeGoal const& goal,
                           ParsedMessage const* question, Player* bot, Player* sender)
{
    switch (intent)
    {
        case Intent::Arena:
        {
            if (!bot)
                return CatalogFact(catalog, "arena_unavailable", locale, {});
            if (question && question->Question && AboutSender(*question))
                return CatalogFact(catalog, "arena_other_unknown", locale, {});
            std::optional<std::uint8_t> requested;
            constexpr std::array<std::array<char const*, 3>, 3> brackets =
                {{{"2v2", "twos", "2s"}, {"3v3", "threes", "3s"}, {"5v5", "fives", "5s"}}};
            if (question)
                for (std::uint8_t slot = 0; slot < brackets.size(); ++slot)
                    for (auto* word : brackets[slot])
                        if (std::find(question->Tokens.begin(), question->Tokens.end(), word) != question->Tokens.end())
                        {
                            if (requested && *requested != slot)
                                return CatalogFact(catalog, "arena_choose_bracket", locale, {});
                            requested = slot;
                        }
            ArenaTeam* selected = nullptr;
            std::uint8_t selectedSlot = 0;
            for (std::uint8_t slot = 0; slot < 3; ++slot)
                if ((!requested || *requested == slot) && bot->GetArenaTeamId(slot))
                    if (auto* team = sArenaTeamMgr->GetArenaTeamById(bot->GetArenaTeamId(slot)))
                    {
                        if (selected)
                            return CatalogFact(catalog, "arena_choose_bracket", locale, {});
                        selected = team;
                        selectedSlot = slot;
                    }
            if (!selected)
                return requested
                    ? CatalogFact(catalog, "arena_no_bracket_team", locale,
                                  {*requested == 0 ? "2v2" : *requested == 1 ? "3v3" : "5v5"})
                    : CatalogFact(catalog, "arena_no_team", locale, {});
            return CatalogFact(catalog, "arena_team_rating", locale,
                               {std::to_string(selected->GetType()) + 'v' + std::to_string(selected->GetType()),
                                Label(selected->GetName()),
                                std::to_string(selected->GetRating()),
                                std::to_string(bot->GetArenaPersonalRating(selectedSlot))});
        }
        case Intent::Battleground:
        {
            if (bot)
                if (auto* bg = bot->GetBattleground(); bg && !bg->isArena())
                    return CatalogFact(catalog, bg->GetStatus() == STATUS_IN_PROGRESS ? "battleground_playing"
                                                : bg->GetStatus() == STATUS_WAIT_LEAVE ? "battleground_finished"
                                                                                     : "battleground_waiting",
                                       locale, {Label(bg->GetName())});
            return CatalogFact(catalog, "battleground_outside", locale, {});
        }
        case Intent::Progress:
            return std::to_string(snapshot.Level);
        case Intent::GroupOffer:
            return CatalogFact(catalog, "entity", locale,
                               {Label(snapshot.HasQuest ? snapshot.QuestName : snapshot.ZoneName)});
        case Intent::Introduction:
            return CatalogFact(catalog, "introduction", locale,
                               {snapshot.Name, std::to_string(snapshot.Level), Label(snapshot.ClassName)});
        case Intent::Location:
            return snapshot.ZoneName.empty()
                       ? ""
                       : CatalogFact(catalog, "entity_sentence", locale, {Label(snapshot.ZoneName)});
        case Intent::Class:
            if (question && question->Question && AboutSender(*question))
                return CatalogFact(catalog, "class_other_unknown", locale, {});
            return CatalogFact(catalog, "class_level", locale,
                               {Label(snapshot.ClassName), std::to_string(snapshot.Level), Label(snapshot.SpecName)});
        case Intent::Quest:
            return snapshot.HasQuest && !snapshot.QuestName.empty()
                       ? CatalogFact(catalog, "entity_sentence", locale, {Label(snapshot.QuestName)})
                       : "";
        case Intent::Equipment:
            if (question && AboutSender(*question))
            {
                if (!bot || !sender || bot->GetMap() != sender->GetMap() || !bot->InSamePhase(sender) ||
                    !bot->IsWithinDistInMap(sender, 10.0f) || !bot->CanSeeOrDetect(sender, false, true) ||
                    !bot->IsWithinLOSInMap(sender))
                    return CatalogFact(catalog, "equipment_other_unknown", locale, {});
                auto* weapon = sender->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                if (!weapon)
                    return CatalogFact(catalog, "equipment_other_unarmed", locale, {});
                return CatalogFact(catalog, "equipment_other_weapon", locale,
                                   {Label(weapon->GetTemplate()->Name1),
                                    std::to_string(static_cast<std::uint16_t>(std::clamp(
                                        sender->GetAverageItemLevel(), 0.0f, 65535.0f)))});
            }
            return snapshot.WeaponName.empty()
                       ? CatalogFact(catalog, "equipment_unarmed", locale, {std::to_string(snapshot.Durability)})
                       : CatalogFact(catalog, "equipment_weapon", locale,
                                     {Label(snapshot.WeaponName), std::to_string(snapshot.AverageItemLevel),
                                      std::to_string(snapshot.Durability)});
        case Intent::Guild:
            return snapshot.GuildName.empty() ? CatalogFact(catalog, "guild_none", locale, {})
                                             : CatalogFact(catalog, "guild_member", locale, {Label(snapshot.GuildName)});
        default:
        {
            auto source = ChatRules::Facts(intent, snapshot, goal);
            return source.empty() ? "" : CatalogLiteral(catalog, source, locale);
        }
    }
}

std::optional<std::string> QuestFacts(Player* bot, std::uint32_t questId, QuestSubject subject)
{
    return QuestFacts(CurrentCatalog(), TEXT_LOCALE, bot, questId, subject);
}

std::optional<std::string> QuestFacts(CatalogHandle const& catalog, std::uint32_t locale, Player* bot,
                                      std::uint32_t questId, QuestSubject subject)
{
    auto* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!bot || !quest)
        return std::nullopt;
    auto title = Label(quest->GetTitle());
    auto status = bot->GetQuestStatus(questId);
    if (subject == QuestSubject::Level)
        return quest->GetQuestLevel() > 0
                   ? CatalogFact(catalog, "quest_level", locale,
                                 {title, std::to_string(quest->GetMinLevel()), std::to_string(quest->GetQuestLevel())})
                   : CatalogFact(catalog, "quest_scaling", locale, {title, std::to_string(quest->GetMinLevel())});
    if (subject == QuestSubject::Directions)
    {
        if (bot->FindQuestSlot(questId) != MAX_QUEST_LOG_SIZE)
            if (auto* markers = sObjectMgr->GetQuestPOIVector(questId))
                for (auto const& marker : *markers)
                    if ((status == QUEST_STATUS_COMPLETE ? marker.ObjectiveIndex < 0 : marker.ObjectiveIndex >= 0) &&
                        !marker.points.empty())
                    {
                        if (marker.MapId != bot->GetMapId())
                            return CatalogFact(catalog, "quest_other_map", locale, {title});
                        float north = static_cast<float>(marker.points.front().x) - bot->GetPositionX();
                        float west = static_cast<float>(marker.points.front().y) - bot->GetPositionY();
                        float distance = std::hypot(north, west);
                        if (distance < 40.0f)
                            return CatalogFact(catalog, "quest_close", locale, {title});
                        std::string direction;
                        if (std::abs(north) * 2.0f >= std::abs(west))
                            direction = north >= 0.0f ? "north" : "south";
                        if (std::abs(west) * 2.0f >= std::abs(north))
                            direction += west >= 0.0f ? "west" : "east";
                        auto approximate = static_cast<std::uint32_t>(std::round(distance / 25.0f) * 25.0f);
                        return CatalogFact(
                            catalog, "quest_directions", locale,
                            {title, std::to_string(approximate), CatalogFact(catalog, direction, locale)});
                    }
        return CatalogFact(catalog, "quest_no_route", locale, {title});
    }
    if (status == QUEST_STATUS_COMPLETE)
        return CatalogFact(catalog, "quest_complete", locale, {title});
    if (bot->GetQuestRewardStatus(questId))
        return CatalogFact(catalog, "quest_rewarded", locale, {title});
    auto progress = bot->getQuestStatusMap().find(questId);
    if (status != QUEST_STATUS_INCOMPLETE || progress == bot->getQuestStatusMap().end())
        return CatalogFact(catalog, "quest_unknown", locale, {title});
    for (std::size_t index = 0; index < QUEST_ITEM_OBJECTIVES_COUNT; ++index)
        if (auto item = quest->RequiredItemId[index];
            item && progress->second.ItemCount[index] < quest->RequiredItemCount[index])
            if (auto* object = sObjectMgr->GetItemTemplate(item))
                return CatalogFact(catalog, "quest_objective", locale,
                                   {title, std::to_string(progress->second.ItemCount[index]),
                                    std::to_string(quest->RequiredItemCount[index]), Label(object->Name1)});
    for (std::size_t index = 0; index < QUEST_OBJECTIVES_COUNT; ++index)
        if (auto entry = quest->RequiredNpcOrGo[index];
            entry && progress->second.CreatureOrGOCount[index] < quest->RequiredNpcOrGoCount[index])
        {
            std::string objective;
            if (entry > 0)
            {
                if (auto* creature = sObjectMgr->GetCreatureTemplate(static_cast<uint32>(entry)))
                    objective = creature->Name;
            }
            else if (auto* object =
                         sObjectMgr->GetGameObjectTemplate(static_cast<uint32>(-static_cast<std::int64_t>(entry))))
                objective = object->name;
            if (!objective.empty())
                return CatalogFact(catalog, "quest_objective", locale,
                                   {title, std::to_string(progress->second.CreatureOrGOCount[index]),
                                    std::to_string(quest->RequiredNpcOrGoCount[index]), Label(objective)});
        }
    return CatalogFact(catalog, "quest_active", locale, {title});
}
}  // namespace Advai
