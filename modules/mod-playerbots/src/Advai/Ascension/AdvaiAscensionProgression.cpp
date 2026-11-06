/*
 * This file is part of mod-playerbots; GNU GPL v2 or later.
 * Advancement gate semantics adapted from AscensionCoATalentData/State, GNU AGPL v3,
 * jealous-sound/azerothcore-wotlk-coa @ 0487265f863b9c50e7757e1f113169b80505ba8b.
 */

#include "AdvaiAscensionProgression.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AdvaiAscensionData.h"
#include "AdvaiAscensionIdentity.h"
#include "AdvaiMgr.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace Advai::Ascension
{
namespace
{
constexpr char StateKey[] = "advai.ascension.state.v1";
constexpr char RankKey[] = "advai.ascension.ranks.0fb5939e2118e804";

uint32 Class(Player* player)
{
    if (!player)
        return 0;

    auto profile = Manager::Instance().Profile(player->GetGUID().GetCounter());
    if (player->getClass() == CLASS_CULTIST && profile == ClassProfile::Cultist)
        return CLASS_CULTIST;
    if (player->getClass() == CLASS_NECROMANCER && profile == ClassProfile::Necromancer)
        return CLASS_NECROMANCER;
    return 0;
}

Data::Node const* Node(uint32 id)
{
    auto found = std::lower_bound(Data::Nodes.begin(), Data::Nodes.end(), id,
                                  [](Data::Node const& node, uint32 value) { return node.Id < value; });
    return found != Data::Nodes.end() && found->Id == id ? &*found : nullptr;
}

uint32 Rank(Player* player, Data::Node const& node)
{
    uint32 index = uint32(&node - Data::Nodes.data());
    uint32 rank = player->GetPlayerSetting(RankKey, index).value;
    if (rank > node.Ranks.size() || (rank && !node.Ranks[rank - 1]))
        throw std::runtime_error("ADVAI Ascension persisted advancement rank is invalid");
    if (rank && !player->HasSpell(node.Ranks[rank - 1]))
        throw std::runtime_error("ADVAI Ascension advancement receipt has no corresponding learned spell");
    return rank;
}

bool Identity(uint32 id)
{
    return std::any_of(Data::Specs.begin(), Data::Specs.end(),
                       [id](Data::Spec const& spec) { return spec.Identity == id; });
}

uint32 RequiredLevel(Data::Node const& node)
{
    // These are the explicit specialization gates in the pinned class implementation.
    return Identity(node.Id) && node.Id != 4041 ? 10 : node.Level;
}

bool Ready(Player* player, Data::Node const& node, uint32 spec, std::unordered_map<uint32, uint32> const& ranks)
{
    if (node.Level > player->GetLevel() || (node.Spec && node.Spec != spec) || RequiredLevel(node) > player->GetLevel())
        return false;
    if (Identity(node.Id) && node.Id != 4041)
        return true;
    bool automatic = !node.AE && !node.TE && !node.Group;
    for (uint32 required : node.Required)
    {
        if (!required)
            continue;
        auto dependency = Node(required);
        if (!dependency)
            throw std::runtime_error("ADVAI Ascension advancement prerequisite is absent");
        // Automatic specialization passives drop dependencies on a purchased shared
        // node, exactly as AscensionCoATalentData's automatic dependency compiler does.
        if (automatic && node.Spec && !dependency->Spec && (dependency->AE || dependency->TE))
            continue;
        auto known = ranks.find(required);
        if (known == ranks.end() || !known->second)
            return false;
    }
    return true;
}
}  // namespace

uint32 Specialization(Player* player)
{
    if (!Enabled() || !player || !Manager::Instance().Applies(player) || !Class(player))
        return 0;
    return player->GetPlayerSetting(StateKey, 1).value;
}

void SeedClass(Player* player)
{
    if (!Enabled() || !player || !Manager::Instance().Applies(player))
        return;
    uint32 cls = Class(player);
    if (!cls)
        return;
    if (Manager::Instance().Initialized(player->GetGUID().GetCounter()) || player->GetPlayerSetting(StateKey, 0).value)
        throw std::runtime_error("ADVAI refused repeated Ascension character initialization");
    std::vector<Data::Spec const*> choices;
    for (auto const& spec : Data::Specs)
        if (spec.Class == cls)
            choices.push_back(&spec);
    auto persona = Manager::Instance().Persona(player->GetGUID().GetCounter());
    auto spec = choices.at(persona.Seed % choices.size());
    player->UpdatePlayerSetting(StateKey, 1, spec->Id);
    // Only faction-neutral proficiencies and the real class starting kit are seeded.
    // Racial spells remain owned by the racial creation path.
    for (uint32 spell : {81u, 107u, 227u, 1180u, 5009u, 5019u, 6603u, 9078u})
        player->learnSpell(spell, false);
    if (cls == CLASS_NECROMANCER)
        for (uint32 spell : {804360u, 500970u, 804558u})
            player->learnSpell(spell, false);
    else
        for (uint32 spell : {196u, 197u, 198u, 199u, 200u, 201u, 202u, 750u, 8737u, 9077u, 9116u, 500720u, 800413u})
            player->learnSpell(spell, false);
    player->UpdatePlayerSetting(StateKey, 0, 1);
    AdvanceClass(player, 256);
}

uint32 AdvanceClass(Player* player, uint32 maximum)
{
    if (!Enabled() || !player || !Manager::Instance().Applies(player) || !Class(player))
        return 0;
    if (player->GetPlayerSetting(StateKey, 0).value != 1)
        throw std::runtime_error("ADVAI Ascension character requires explicit class conversion before gameplay");
    uint32 cls = Class(player), spec = Specialization(player);
    auto specialization = std::find_if(Data::Specs.begin(), Data::Specs.end(), [cls, spec](Data::Spec const& item)
                                       { return item.Class == cls && item.Id == spec; });
    if (specialization == Data::Specs.end())
        throw std::runtime_error("ADVAI Ascension character has an invalid persistent specialization");
    auto budget = std::find_if(Data::Budgets.begin(), Data::Budgets.end(), [cls, player](Data::Budget const& item)
                               { return item.Class == cls && item.Level == player->GetLevel(); });
    if (budget == Data::Budgets.end())
        throw std::runtime_error("ADVAI Ascension character has no level budget");
    std::unordered_map<uint32, uint32> ranks, groups;
    std::unordered_set<uint32> charged;
    uint32 spentAE = 0, spentTE = 0;
    std::vector<Data::Node const*> candidates;
    for (auto const& node : Data::Nodes)
    {
        if (node.Class != cls)
            continue;
        uint32 rank = Rank(player, node);
        ranks[node.Id] = rank;
        if (rank && node.Spec && node.Spec != spec)
            throw std::runtime_error("ADVAI Ascension character mixes mutually exclusive specializations");
        if (rank && node.Group && !node.AE && !node.TE)
        {
            if (groups.count(node.Group) && groups[node.Group] != node.Id)
                throw std::runtime_error("ADVAI Ascension character has conflicting advancement choices");
            groups[node.Group] = node.Id;
        }
        for (uint32 index = 0; index < rank; ++index)
            if (charged.insert(node.Ranks[index]).second)
            {
                spentAE += node.Spec ? 0 : node.AE;
                spentTE += node.Spec ? node.TE : 0;
            }
        if (!node.Spec || node.Spec == spec)
            candidates.push_back(&node);
    }
    if (spentAE > budget->AE || spentTE > budget->TE)
        throw std::runtime_error("ADVAI Ascension character exceeds its earned advancement budget");
    uint32 seed = Manager::Instance().Persona(player->GetGUID().GetCounter()).Seed;
    auto priority = [seed, specialization](Data::Node const* node)
    {
        bool signature =
            std::find(node->Ranks.begin(), node->Ranks.end(), specialization->Signature) != node->Ranks.end();
        return std::pair(!signature, (node->Id * 2654435761u) ^ seed);
    };
    std::sort(candidates.begin(), candidates.end(),
              [&](auto left, auto right) { return priority(left) < priority(right); });
    uint32 learned = 0;
    bool changed = true;
    for (std::size_t pass = 0; changed && pass < Data::Nodes.size() && learned < maximum; ++pass)
    {
        changed = false;
        for (auto node : candidates)
        {
            uint32 rank = ranks[node->Id];
            if (learned >= maximum || rank >= node->Ranks.size() || !node->Ranks[rank] ||
                !Ready(player, *node, spec, ranks) ||
                (node->Group && !node->AE && !node->TE && groups.count(node->Group) && groups[node->Group] != node->Id))
                continue;
            uint32 spell = node->Ranks[rank];
            uint32 ae = node->Spec || charged.count(spell) ? 0 : node->AE;
            uint32 te = !node->Spec || charged.count(spell) ? 0 : node->TE;
            if (spentAE + ae > budget->AE || spentTE + te > budget->TE)
                continue;
            auto info = sSpellMgr->GetSpellInfo(spell);
            if (!info)
                throw std::runtime_error("ADVAI Ascension advancement spell is absent");
            player->learnSpell(spell, false);
            if (!player->HasSpell(spell))
                throw std::runtime_error("ADVAI Ascension advancement was rejected by native spell learning");
            ++rank;
            player->UpdatePlayerSetting(RankKey, uint32(node - Data::Nodes.data()), rank);
            ranks[node->Id] = rank;
            if (node->Group && !node->AE && !node->TE)
                groups[node->Group] = node->Id;
            charged.insert(spell);
            spentAE += ae;
            spentTE += te;
            ++learned;
            changed = true;
        }
    }
    return learned;
}
}  // namespace Advai::Ascension
