/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#include "AdvaiPvp.h"

#include <algorithm>
#include <cctype>
#include <limits>

#include "AdvaiMgr.h"
#include "AiObjectContext.h"
#include "Battleground.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Spell.h"
#include "SpellInfo.h"

namespace Advai
{
namespace
{
bool CarriesFlag(Unit* player)
{
    return player->HasAura(23333) || player->HasAura(23335) || player->HasAura(34976);
}

bool HealingInSight(Unit* player)
{
    auto* cast = player->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    if (!cast)
        cast = player->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
    if (!cast)
        return false;
    for (auto const& effect : cast->GetSpellInfo()->Effects)
        if (effect.Effect == SPELL_EFFECT_HEAL || effect.Effect == SPELL_EFFECT_HEAL_PCT ||
            effect.ApplyAuraName == SPELL_AURA_PERIODIC_HEAL)
            return true;
    return false;
}

std::vector<Player*> VisibleAllies(PlayerbotAI* ai, float range)
{
    Player* bot = ai->GetBot();
    std::vector<Player*> allies;
    if (auto* group = bot->GetGroup())
        for (auto* ref = group->GetFirstMember(); ref && allies.size() < 40; ref = ref->next())
            if (auto* member = ref->GetSource(); member && member != bot && member->IsAlive() &&
                member->IsInWorld() && member->GetMap() == bot->GetMap() && bot->InSamePhase(member) &&
                member->GetBattlegroundId() == bot->GetBattlegroundId() &&
                member->GetBgTeamId() == bot->GetBgTeamId() && bot->IsWithinDistInMap(member, range) &&
                bot->CanSeeOrDetect(member) && bot->IsWithinLOSInMap(member))
                allies.push_back(member);
    return allies;
}
}

bool IsPvpIntent(Intent topic)
{
    return topic == Intent::PvpFocus || topic == Intent::PvpRegroup || topic == Intent::PvpPeel;
}

bool VisiblePvpEnemy(Player* bot, Unit* target, float range)
{
    return bot && target && target->IsPlayer() && target->IsAlive() && target->IsInWorld() &&
        target->GetMap() == bot->GetMap() && bot->InSamePhase(target) &&
        bot->IsWithinDistInMap(target, range) && bot->CanSeeOrDetect(target, false, true) &&
        bot->IsWithinLOSInMap(target) && bot->IsValidAttackTarget(target) &&
        !target->HasSpiritOfRedemptionAura();
}

Unit* ChoosePvpTarget(PlayerbotAI* ai, std::vector<Unit*> const& candidates, Unit* current)
{
    Player* bot = ai->GetBot();
    auto persona = Manager::Instance().Persona(bot->GetGUID().GetCounter());
    auto suggested = Manager::Instance().PvpSuggestedTarget(bot->GetGUID().GetCounter());
    auto allies = VisibleAllies(ai, 45.0f);
    Player* shotCaller = nullptr;
    if (bot->GetBattleground() && bot->GetBattleground()->isArena())
        for (auto* ally : allies)
            if (!PlayerbotAI::IsHeal(ally) && ally->GetVictim() &&
                (!shotCaller || ally->GetGUID().GetCounter() < shotCaller->GetGUID().GetCounter()))
                shotCaller = ally;
    if (shotCaller && !PlayerbotAI::IsHeal(bot) && bot->GetGUID().GetCounter() < shotCaller->GetGUID().GetCounter())
        shotCaller = bot;
    Unit* best = nullptr;
    float bestScore = -std::numeric_limits<float>::max();
    std::unordered_set<std::uint64_t> seen;
    for (auto* enemy : candidates)
    {
        if (!enemy || !seen.insert(enemy->GetGUID().GetRawValue()).second ||
            !VisiblePvpEnemy(bot, enemy) || enemy->HasBreakableByDamageCrowdControlAura() ||
            enemy->HasAura(642) || enemy->HasAura(45438) || enemy->HasAura(33786))
            continue;
        float score = 35.0f - bot->GetDistance(enemy) * (PlayerbotAI::IsRanged(bot) ? 0.5f : 0.9f);
        score += (100.0f - enemy->GetHealthPct()) * (0.3f + persona.Competitiveness / 200.0f);
        if (enemy == current)
            score += 22.0f; // Switching needs a tangible benefit rather than a new nearest target.
        if (suggested && enemy->GetGUID().GetCounter() == suggested)
            score += 45.0f;
        if (CarriesFlag(enemy))
            score += 100.0f;
        if (shotCaller && shotCaller->GetVictim() == enemy)
            score += 30.0f; // Follow an observable teammate's focus, retaining immunity/CC checks above.
        if (HealingInSight(enemy))
            score += 15.0f + persona.Skill * 0.4f;
        if (enemy->GetVictim() == bot)
            score += 8.0f;
        for (auto* ally : allies)
        {
            if (ally->GetVictim() == enemy)
                score += 9.0f + persona.Skill / 12.0f;
            if (enemy->GetVictim() == ally &&
                (ally->GetHealthPct() < 50.0f || CarriesFlag(ally) || HealingInSight(ally)))
                score += 25.0f + persona.Generosity / 4.0f;
        }
        score += (persona.Seed ^ enemy->GetGUID().GetCounter()) % 11;
        if (score > bestScore)
        {
            bestScore = score;
            best = enemy;
        }
    }
    return best;
}

Player* PvpObjectiveAnchor(PlayerbotAI* ai)
{
    auto* bot = ai->GetBot();
    auto* bg = bot->GetBattleground();
    if (!bg || bg->isArena() || bg->GetStatus() != STATUS_IN_PROGRESS ||
        ai->HasGameClientMaster() || !bot->IsAlive() || CarriesFlag(bot))
        return nullptr;
    auto allies = VisibleAllies(ai, 60.0f);
    for (auto* carrier : allies)
        if (CarriesFlag(carrier))
        {
            auto priority = [carrier](Player* ally)
                { return ally->GetDistance(carrier) - (PlayerbotAI::IsHeal(ally) ? 15.0f : 0.0f); };
            auto ownPriority = priority(bot);
            auto ahead = std::count_if(allies.begin(), allies.end(), [&](auto* ally)
                { return ally != carrier && !CarriesFlag(ally) &&
                    (priority(ally) < ownPriority || (priority(ally) == ownPriority &&
                     ally->GetGUID().GetCounter() < bot->GetGUID().GetCounter())); });
            if (ahead < 2 && !bot->IsWithinDistInMap(carrier, 12.0f))
                return carrier;
        }
    return nullptr;
}

Player* PvpRegroupAnchor(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    auto* bg = bot->GetBattleground();
    if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS || ai->HasGameClientMaster() ||
        !bot->IsAlive() || bot->GetVehicle() || bot->IsNonMeleeSpellCast(false) || CarriesFlag(bot))
        return nullptr;
    auto persona = Manager::Instance().Persona(bot->GetGUID().GetCounter());
    if (bot->GetHealthPct() >= 35.0f + (100 - persona.RiskTolerance) / 4.0f)
        return nullptr;
    auto allies = VisibleAllies(ai, 60.0f);
    std::vector<Unit*> enemies;
    for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get())
        if (auto* enemy = ai->GetUnit(guid); VisiblePvpEnemy(bot, enemy, 40.0f))
        {
            enemies.push_back(enemy);
            if (enemies.size() == 40)
                break;
        }
    auto nearby = std::count_if(allies.begin(), allies.end(),
        [bot](auto* ally) { return bot->IsWithinDistInMap(ally, 20.0f); });
    if (enemies.size() < static_cast<std::size_t>(nearby + 2))
        return nullptr;
    Player* best = nullptr;
    float bestScore = 0;
    for (auto* ally : allies)
    {
        auto distance = bot->GetDistance(ally);
        if (distance < 12.0f || distance > 55.0f || ally->GetHealthPct() < 35.0f)
            continue;
        float ownThreat = std::numeric_limits<float>::max(), friendThreat = ownThreat;
        for (auto* enemy : enemies)
        {
            ownThreat = std::min(ownThreat, bot->GetDistance(enemy));
            friendThreat = std::min(friendThreat, ally->GetDistance(enemy));
        }
        if (friendThreat < ownThreat + 8.0f)
            continue;
        float score = friendThreat - distance * 0.4f + (HealingInSight(ally) ? 15.0f : 0.0f);
        if (score > bestScore)
        {
            bestScore = score;
            best = ally;
        }
    }
    return best;
}

Unit* PvpRequestTarget(PlayerbotAI* ai, Player* sender, ParsedMessage const& message, Intent topic)
{
    auto* bot = ai->GetBot();
    if (!sender || !bot->GetGroup() || bot->GetGroup() != sender->GetGroup() ||
        sender->GetBattlegroundId() != bot->GetBattlegroundId() ||
        sender->GetMap() != bot->GetMap() || sender->GetBgTeamId() != bot->GetBgTeamId())
        return nullptr;
    if (topic == Intent::PvpPeel)
    {
        auto* attacker = sender->getAttackerForHelper();
        return VisiblePvpEnemy(bot, attacker) ? attacker : nullptr;
    }
    if (topic != Intent::PvpFocus)
        return nullptr;
    auto has = [&](char const* word)
        { return std::find(message.Tokens.begin(), message.Tokens.end(), word) != message.Tokens.end(); };
    for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get())
        if (auto* enemy = ai->GetUnit(guid); VisiblePvpEnemy(bot, enemy))
        {
            std::string name = enemy->GetName();
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (std::find(message.Tokens.begin(), message.Tokens.end(), name) != message.Tokens.end())
                return enemy;
        }
    if (has("healer"))
        for (auto guid : ai->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get())
            if (auto* enemy = ai->GetUnit(guid); VisiblePvpEnemy(bot, enemy) && HealingInSight(enemy))
                return enemy;
    if (has("healer"))
        return nullptr; // A request never reveals the enemy's private specialization.
    auto* target = sender->GetVictim();
    return VisiblePvpEnemy(bot, target) ? target : nullptr;
}

std::optional<std::string> PvpFacts(CatalogHandle const& catalog, std::uint32_t locale,
                                  PlayerbotAI* ai, Intent topic, BotId observed)
{
    Player* bot = ai->GetBot();
    auto* bg = bot->GetBattleground();
    if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS || !bot->IsAlive())
        return std::nullopt;
    auto* target = observed ? ObjectAccessor::GetPlayer(bot->GetMap(), ObjectGuid::Create<HighGuid::Player>(observed))
                            : nullptr;
    if (topic == Intent::PvpRegroup)
    {
        auto* anchor = PvpRegroupAnchor(ai);
        if (!anchor || (observed && anchor != target))
            return observed ? std::nullopt : std::optional<std::string>(
                CatalogFact(catalog, "pvp_no_regroup_anchor", locale, {}));
        return CatalogFact(catalog, "pvp_regroup_anchor", locale, {anchor->GetName()});
    }
    if (!target && !observed)
        target = bot->GetVictim() ? bot->GetVictim()->ToPlayer() : nullptr;
    if (!VisiblePvpEnemy(bot, target) || target->HasBreakableByDamageCrowdControlAura() ||
        target->HasAura(642) || target->HasAura(45438) || target->HasAura(33786))
        return observed ? std::nullopt : std::optional<std::string>(
            CatalogFact(catalog, "pvp_no_clear_target", locale, {}));
    if (topic == Intent::PvpPeel)
    {
        auto* ally = target->GetVictim() ? target->GetVictim()->ToPlayer() : nullptr;
        if (!ally || ally == bot || !bot->GetGroup() || ally->GetGroup() != bot->GetGroup() || !ally->IsAlive() ||
            ally->GetBgTeamId() != bot->GetBgTeamId() || !bot->CanSeeOrDetect(ally) ||
            !bot->IsWithinLOSInMap(ally))
            return observed ? std::nullopt : std::optional<std::string>(
                CatalogFact(catalog, "pvp_no_clear_peel", locale, {}));
        return CatalogFact(catalog, "pvp_peel_target", locale, {target->GetName(), ally->GetName()});
    }
    return CatalogFact(catalog, "pvp_visible_target", locale, {target->GetName()});
}
}
