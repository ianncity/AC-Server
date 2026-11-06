#ifndef ASCENSION_PROGRESSION_H
#define ASCENSION_PROGRESSION_H

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "AdvaiAscensionData.h"
#include "AdvaiAscensionIdentity.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace Advai::Ascension::Progression
{
constexpr uint32 NecromancerClass = CLASS_NECROMANCER;
constexpr uint32 CultistClass = CLASS_CULTIST;

enum class Result
{
    Success,
    WrongClass,
    AlreadyInitialized,
    NotInitialized,
    SelectSpecializationFirst,
    InvalidSpecialization,
    SpecializationLocked,
    SpecializationChangeLocked,
    SpecializationAlreadyChosen,
    InvalidNode,
    WrongClassNode,
    WrongSpecialization,
    LevelRequired,
    PrerequisiteRequired,
    ChoiceLocked,
    NoAdvancementPoints,
    RankComplete,
    MissingSpell,
    InitializationFailed
};

struct Budget
{
    uint32 AE = 0;
    uint32 TE = 0;
};

struct Spent
{
    uint32 AE = 0;
    uint32 TE = 0;
};

inline uint32 Class(Player const* player)
{
    if (Advai::Ascension::IsCultist(player))
    {
        return CultistClass;
    }
    if (Advai::Ascension::IsNecromancer(player))
    {
        return NecromancerClass;
    }
    return 0;
}

inline Data::Node const* FindNode(uint32 id)
{
    auto found = std::lower_bound(Data::Nodes.begin(), Data::Nodes.end(), id,
        [](Data::Node const& node, uint32 value) { return node.Id < value; });
    return found != Data::Nodes.end() && found->Id == id ? &*found : nullptr;
}

inline bool IsIdentityNode(uint32 id)
{
    return std::any_of(Data::Specs.begin(), Data::Specs.end(),
        [id](Data::Spec const& spec) { return spec.Identity == id; });
}

inline Data::Spec const* FindSpec(uint32 classId, uint32 specId)
{
    auto found = std::find_if(Data::Specs.begin(), Data::Specs.end(),
        [classId, specId](Data::Spec const& spec) { return spec.Class == classId && spec.Id == specId; });
    return found == Data::Specs.end() ? nullptr : &*found;
}

inline uint32 Specialization(Player const* player)
{
    uint32 classId = Class(player);
    uint32 selected = 0;
    for (Data::Spec const& spec : Data::Specs)
    {
        if (spec.Class != classId)
        {
            continue;
        }
        Data::Node const* identity = FindNode(spec.Identity);
        if (!identity || !identity->Ranks[0] || !player->HasSpell(identity->Ranks[0]))
        {
            continue;
        }
        if (selected)
        {
            throw std::runtime_error("Ascension character has multiple specialization markers");
        }
        selected = spec.Id;
    }
    return selected;
}

inline uint32 Rank(Player const* player, Data::Node const& node)
{
    uint32 rank = 0;
    bool gap = false;
    for (uint32 spell : node.Ranks)
    {
        if (!spell)
        {
            gap = true;
            continue;
        }
        bool known = player->HasSpell(spell);
        if (gap && known)
        {
            throw std::runtime_error("Ascension character has a later rank without its earlier rank");
        }
        if (known)
        {
            ++rank;
        }
        else
        {
            // Ranks are learned one at a time, so a known prefix followed by unknown ranks is a
            // valid partial rank. Only a known rank after a missing one is inconsistent.
            gap = true;
        }
    }
    return rank;
}

inline Budget EarnedBudget(Player const* player)
{
    uint32 classId = Class(player);
    auto found = std::find_if(Data::Budgets.begin(), Data::Budgets.end(),
        [classId, player](Data::Budget const& entry) { return entry.Class == classId && entry.Level == player->GetLevel(); });
    if (found == Data::Budgets.end())
    {
        throw std::runtime_error("Ascension class data has no budget for the player's level");
    }
    return { found->AE, found->TE };
}

inline Spent PointsSpent(Player const* player)
{
    uint32 classId = Class(player);
    std::unordered_set<uint32> charged;
    Spent result;
    for (Data::Node const& node : Data::Nodes)
    {
        if (node.Class != classId)
        {
            continue;
        }
        uint32 rank = Rank(player, node);
        for (uint32 index = 0; index < rank; ++index)
        {
            if (charged.insert(node.Ranks[index]).second)
            {
                result.AE += node.Spec ? 0 : node.AE;
                result.TE += node.Spec ? node.TE : 0;
            }
        }
    }
    return result;
}

inline bool Ready(Player const* player, Data::Node const& node, uint32 spec)
{
    uint32 requiredLevel = IsIdentityNode(node.Id) ? 10 : node.Level;
    if (node.Level > player->GetLevel() || requiredLevel > player->GetLevel() || (node.Spec && node.Spec != spec))
    {
        return false;
    }
    if (IsIdentityNode(node.Id))
    {
        return true;
    }

    bool automaticSpecializationPassive = !node.AE && !node.TE && !node.Group && node.Spec;
    for (uint32 required : node.Required)
    {
        if (!required)
        {
            continue;
        }
        Data::Node const* dependency = FindNode(required);
        if (!dependency)
        {
            if (!player->HasSpell(required))
            {
                return false;
            }
            continue;
        }
        if (automaticSpecializationPassive && !dependency->Spec && (dependency->AE || dependency->TE))
        {
            continue;
        }
        if (!Rank(player, *dependency))
        {
            return false;
        }
    }
    return true;
}

inline bool ChoiceAvailable(Player const* player, Data::Node const& node)
{
    if (!node.Group || node.AE || node.TE)
    {
        return true;
    }
    return std::none_of(Data::Nodes.begin(), Data::Nodes.end(), [player, &node](Data::Node const& candidate)
    {
        return candidate.Class == node.Class && candidate.Id != node.Id && candidate.Group == node.Group &&
               (candidate.AE == 0 && candidate.TE == 0) && Rank(player, candidate) > 0;
    });
}

inline Result Initialize(Player* player, uint32 expectedClass)
{
    if (!player || (expectedClass != CultistClass && expectedClass != NecromancerClass) ||
        player->getClass() != expectedClass)
        return Result::WrongClass;

    std::array<uint32, 2> const starterSpells = expectedClass == CultistClass
        ? std::array<uint32, 2>{500720, 800413}
        : std::array<uint32, 2>{500970, 804558};
    for (uint32 spell : starterSpells)
    {
        if (!sSpellMgr->GetSpellInfo(spell))
            return Result::MissingSpell;
        if (!player->HasSpell(spell))
            player->learnSpell(spell, false);
        if (!player->HasSpell(spell))
            return Result::InitializationFailed;

        bool onActionBar = false;
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
        {
            ActionButton const* action = player->GetActionButton(button);
            if (action && action->GetType() == ACTION_BUTTON_SPELL && action->GetAction() == spell)
            {
                onActionBar = true;
                break;
            }
        }
        if (onActionBar)
            continue;

        // Existing characters may have occupied starter slots from an older
        // class layout. Preserve those buttons and place missing starters in
        // the first empty action-bar buttons instead of silently skipping them.
        uint8 emptyButton = MAX_ACTION_BUTTONS;
        for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
            if (!player->GetActionButton(button))
            {
                emptyButton = button;
                break;
            }
        if (emptyButton == MAX_ACTION_BUTTONS || !player->addActionButton(emptyButton, spell, ACTION_BUTTON_SPELL))
            return Result::InitializationFailed;
    }
    return Result::Success;
}

inline Result SelectSpecialization(Player* player, uint32 expectedClass, uint32 specId)
{
    if (!player || Class(player) != expectedClass)
    {
        return Result::NotInitialized;
    }
    if (player->GetLevel() < 10)
    {
        return Result::SpecializationLocked;
    }
    Data::Spec const* spec = FindSpec(expectedClass, specId);
    if (!spec)
    {
        return Result::InvalidSpecialization;
    }
    uint32 selected = Specialization(player);
    if (selected)
    {
        return selected == specId ? Result::SpecializationAlreadyChosen : Result::SpecializationChangeLocked;
    }
    Data::Node const* identity = FindNode(spec->Identity);
    if (!identity || !identity->Ranks[0])
    {
        throw std::runtime_error("Ascension specialization identity node is absent");
    }
    SpellInfo const* info = sSpellMgr->GetSpellInfo(identity->Ranks[0]);
    if (!info)
    {
        return Result::MissingSpell;
    }
    player->learnSpell(identity->Ranks[0], false);
    return Specialization(player) == specId ? Result::Success : Result::MissingSpell;
}

inline Result LearnNode(Player* player, uint32 expectedClass, uint32 nodeId)
{
    if (!player || Class(player) != expectedClass)
    {
        return Result::NotInitialized;
    }
    uint32 spec = Specialization(player);
    if (!spec)
    {
        return Result::SelectSpecializationFirst;
    }
    Data::Node const* node = FindNode(nodeId);
    if (!node)
    {
        return Result::InvalidNode;
    }
    if (node->Class != expectedClass)
    {
        return Result::WrongClassNode;
    }
    if (IsIdentityNode(node->Id))
    {
        return Result::SpecializationAlreadyChosen;
    }
    if (node->Spec && node->Spec != spec)
    {
        return Result::WrongSpecialization;
    }
    uint32 rank = Rank(player, *node);
    if (rank >= node->Ranks.size() || !node->Ranks[rank])
    {
        return Result::RankComplete;
    }
    if (!Ready(player, *node, spec))
    {
        return node->Level > player->GetLevel() ? Result::LevelRequired : Result::PrerequisiteRequired;
    }

    std::unordered_map<uint32, uint32> groups;
    std::unordered_set<uint32> charged;
    for (Data::Node const& entry : Data::Nodes)
    {
        if (entry.Class != expectedClass)
        {
            continue;
        }
        uint32 currentRank = Rank(player, entry);
        if (currentRank && entry.Group && !entry.AE && !entry.TE)
        {
            auto [found, inserted] = groups.emplace(entry.Group, entry.Id);
            if (!inserted && found->second != entry.Id)
            {
                throw std::runtime_error("Ascension character has mutually exclusive choices learned together");
            }
        }
        for (uint32 index = 0; index < currentRank; ++index)
        {
            charged.insert(entry.Ranks[index]);
        }
    }
    if (node->Group && !node->AE && !node->TE && groups.count(node->Group) && groups[node->Group] != node->Id)
    {
        return Result::ChoiceLocked;
    }

    Budget budget = EarnedBudget(player);
    Spent spent = PointsSpent(player);
    uint32 spell = node->Ranks[rank];
    uint32 costAE = node->Spec || charged.count(spell) ? 0 : node->AE;
    uint32 costTE = !node->Spec || charged.count(spell) ? 0 : node->TE;
    if (spent.AE + costAE > budget.AE || spent.TE + costTE > budget.TE)
    {
        return Result::NoAdvancementPoints;
    }
    if (!sSpellMgr->GetSpellInfo(spell))
    {
        return Result::MissingSpell;
    }
    player->learnSpell(spell, false);
    return player->HasSpell(spell) ? Result::Success : Result::MissingSpell;
}

inline void LearnFreePassives(Player* player, uint32 expectedClass)
{
    if (!player || Class(player) != expectedClass)
    {
        return;
    }
    uint32 spec = Specialization(player);
    if (!spec)
    {
        return;
    }
    bool changed = true;
    for (std::size_t pass = 0; changed && pass < Data::Nodes.size(); ++pass)
    {
        changed = false;
        for (Data::Node const& node : Data::Nodes)
        {
            if (node.Class != expectedClass || IsIdentityNode(node.Id) || node.AE || node.TE || node.Group ||
                (node.Spec && node.Spec != spec))
            {
                continue;
            }
            uint32 rank = Rank(player, node);
            if (rank >= node.Ranks.size() || !node.Ranks[rank] || !Ready(player, node, spec))
            {
                continue;
            }
            uint32 spell = node.Ranks[rank];
            if (!sSpellMgr->GetSpellInfo(spell))
            {
                throw std::runtime_error("Ascension free passive is missing spell " + std::to_string(spell));
            }
            player->learnSpell(spell, false);
            if (!player->HasSpell(spell))
            {
                throw std::runtime_error("Ascension free passive was rejected by native spell learning");
            }
            changed = true;
        }
    }
}

inline void Reset(Player* player, uint32 expectedClass)
{
    if (!player || Class(player) != expectedClass)
    {
        return;
    }
    for (Data::Node const& node : Data::Nodes)
    {
        if (node.Class != expectedClass)
        {
            continue;
        }
        for (uint32 spell : node.Ranks)
        {
            if (spell && player->HasSpell(spell))
            {
                player->removeSpell(spell, SPEC_MASK_ALL, false);
            }
        }
    }
}
} // namespace Advai::Ascension::Progression

#endif
