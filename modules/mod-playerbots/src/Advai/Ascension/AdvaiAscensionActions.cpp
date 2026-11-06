/* This file is part of mod-playerbots; GNU GPL v2 or later. */

#include <algorithm>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

#include "AdvaiAscensionData.h"
#include "AdvaiAscensionIdentity.h"
#include "AdvaiAscensionProgression.h"
#include "AdvaiMovement.h"
#include "AiObjectContext.h"
#include "AscensionNecromancer.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

namespace Advai::Ascension
{
namespace
{
struct CastCount
{
    uint64 Attempts = 0, Casts = 0, NoPower = 0, Suppressed = 0;
    std::map<int, uint64> FailCodes;  // SpellCastResult of CheckCast for failed candidates
    std::string Name;
};
std::mutex castMutex;
std::map<std::pair<uint8, uint32>, CastCount> castCounts;  // (class, spell)
std::map<uint32, time_t> spellBackoff;  // (bot guid << 0) -> do not retry a failed summon before this time
std::atomic<uint64> actCalls{0}, actNoCandidate{0}, actCombatNoEnemy{0}, actInCombat{0};
}  // namespace

std::string failText(CastCount const& count)
{
    std::string text;
    for (auto const& [code, n] : count.FailCodes)
        text += " fail" + std::to_string(code) + "=" + std::to_string(n);
    return text;
}

std::string CastReport()
{
    std::lock_guard<std::mutex> lock(castMutex);
    std::string out = "Act calls=" + std::to_string(actCalls.load()) + " inCombat=" + std::to_string(actInCombat.load()) +
                      " combatNoEnemy=" + std::to_string(actCombatNoEnemy.load()) +
                      " noCandidate=" + std::to_string(actNoCandidate.load()) + "\n";
    for (auto const& [key, count] : castCounts)
        out += (key.first == 12 ? "Cultist " : "Necro ") + std::to_string(key.second) + " " + count.Name +
               " attempts=" + std::to_string(count.Attempts) + " casts=" + std::to_string(count.Casts) +
               " noPower=" + std::to_string(count.NoPower) + " suppressed=" + std::to_string(count.Suppressed) + failText(count) + "\n";
    return out;
}
void ResetCastReport()
{
    std::lock_guard<std::mutex> lock(castMutex);
    castCounts.clear();
    actCalls = actNoCandidate = actCombatNoEnemy = actInCombat = 0;
}

// Diagnostic: why does this bot cast (or not cast) its class spells? One line per known active spell with the
// reason it is dropped by Act()'s candidate filter, so rotation gaps can be seen instead of guessed.
namespace
{
struct ActionCache
{
    size_t SpellBookSize = 0;
    std::vector<std::pair<uint32, SpellInfo const*>> Actions;
};
std::mutex actionCacheMutex;
std::unordered_map<uint32, ActionCache> actionCache;
}  // namespace

// Known, non-passive spells of the custom classes' spell families. The returned reference is only used on the
// bot's own update (one thread per map), so the per-bot entry is stable for the duration of the call.
std::vector<std::pair<uint32, SpellInfo const*>> const& ClassActions(Player* bot)
{
    std::lock_guard<std::mutex> lock(actionCacheMutex);
    ActionCache& cache = actionCache[bot->GetGUID().GetCounter()];
    if (cache.SpellBookSize != bot->GetSpellMap().size())
    {
        cache.SpellBookSize = bot->GetSpellMap().size();
        cache.Actions.clear();
        for (auto const& [spell, entry] : bot->GetSpellMap())
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(spell);
            if (info && !info->IsPassive() && (info->SpellFamilyName == 29 || info->SpellFamilyName == 31))
                cache.Actions.emplace_back(spell, info);
        }
    }
    return cache.Actions;
}

std::string Explain(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    std::string out;
    uint32 total = 0, passive = 0, wrongFamily = 0, notAuthored = 0, cooldown = 0, eligible = 0;
    Unit* enemy = ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    out += "spec=" + std::to_string(Specialization(bot)) + " level=" + std::to_string(bot->GetLevel()) +
           " combat=" + std::to_string(bot->IsInCombat()) + " target=" + (enemy ? enemy->GetName() : "none") + "\n";
    {
        Aura* lifeForce = bot->GetAura(525004);
        Aura* capacity = bot->GetAura(805011);
        out += "auras: 525004(stacks)=" + std::to_string(lifeForce ? lifeForce->GetStackAmount() : 0) +
               " 805011(stacks)=" + std::to_string(capacity ? capacity->GetStackAmount() : 0) +
               " power=" + std::to_string(bot->GetPower(bot->getPowerType())) + "/" +
               std::to_string(bot->GetMaxPower(bot->getPowerType())) +
               (bot->getClass() == 13 ? " minions=" + std::to_string(AscensionNecromancer::State(bot).minions.size()) : "") +
               "\n";
    }
    for (auto const& [spell, entry] : bot->GetSpellMap())
    {
        if (!bot->HasActiveSpell(spell))
            continue;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell);
        if (!info)
            continue;
        ++total;
        if (info->IsPassive())
        {
            ++passive;
            continue;
        }
        bool family = info->SpellFamilyName == 29 || info->SpellFamilyName == 31;
        bool authored = std::any_of(Data::Nodes.begin(), Data::Nodes.end(), [spell](auto const& node)
                                    { return std::find(node.Ranks.begin(), node.Ranks.end(), spell) != node.Ranks.end(); });
        bool starter = spell == 500720 || spell == 800413 || spell == 804558 || spell == 500970;
        if (spell < 300000)
            continue;  // donor ranks / generic: ignore in the report
        char line[220];
        snprintf(line, sizeof(line), "%u %s family=%u authored=%d starter=%d cd=%d dmg=%d => %s\n", spell,
                 info->SpellName[0], info->SpellFamilyName, authored, starter, bot->HasSpellCooldown(spell),
                 info->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) || info->HasAura(SPELL_AURA_PERIODIC_DAMAGE),
                 !family ? "DROPPED(family)" : (!authored && !starter) ? "DROPPED(not authored)" : "eligible");
        out += line;
        if (!family) ++wrongFamily; else if (!authored && !starter) ++notAuthored; else if (bot->HasSpellCooldown(spell)) ++cooldown; else ++eligible;
    }
    out += "known=" + std::to_string(total) + " passive=" + std::to_string(passive) + " droppedByFamily=" +
           std::to_string(wrongFamily) + " droppedNotAuthored=" + std::to_string(notAuthored) +
           " eligible=" + std::to_string(eligible) + "\n";
    return out;
}

bool Act(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    if (!Enabled() || !ai->AllowActivity() || !bot->IsAlive() || bot->IsInFlight())
        return false;
    uint32 spec = Specialization(bot);
    bool healer = spec == 40, tank = spec == 96;
    Unit* enemy = ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (enemy && (!enemy->IsAlive() || !bot->IsValidAttackTarget(enemy) || !bot->CanSeeOrDetect(enemy)))
        enemy = nullptr;
    ++actCalls;
    if (bot->IsInCombat())
    {
        ++actInCombat;
        if (!enemy)
        {
            ++actCombatNoEnemy;
            // The shared "current target" value is only set by the generic attack actions. These classes act from this
            // function, so fall back to who the bot is fighting or who is attacking it.
            auto usable = [&](Unit* unit)
            { return unit && unit->IsAlive() && bot->IsValidAttackTarget(unit) && bot->CanSeeOrDetect(unit); };
            if (usable(bot->GetVictim()))
                enemy = bot->GetVictim();
            else if (Unit* selected = bot->GetSelectedUnit(); usable(selected) && selected->IsInCombat())
                enemy = selected;
            else
                for (Unit* attacker : bot->getAttackers())
                    if (usable(attacker))
                    {
                        enemy = attacker;
                        break;
                    }
            if (enemy)
                bot->SetSelection(enemy->GetGUID());
        }
    }
    Unit* wounded = bot;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource();
                member && member->IsAlive() && member->IsInMap(bot) && bot->IsWithinDistInMap(member, 35) &&
                member->GetHealthPct() < wounded->GetHealthPct() && bot->IsWithinLOSInMap(member))
                wounded = member;
    struct Candidate
    {
        uint32 Spell;
        Unit* Target;
        int Score;
    };
    std::vector<Candidate> candidates;
    // CPU: most bots have 150-350 known spells of which ~20-40 are class actions, and this runs several times a second
    // per bot. Cache the class-action list per bot and rebuild only when the spell book changes size.
    std::vector<std::pair<uint32, SpellInfo const*>> const& actions = ClassActions(bot);
    for (auto const& [spell, info] : actions)
    {
        if (!bot->HasActiveSpell(spell) || bot->HasSpellCooldown(spell))
            continue;
        // Previously only talent-node spells plus four starters were allowed, which dropped the classes' primary
        // trainer-taught attacks and heals (Eldritch Blast, Horrorbolt, Eldritch Mending). Every known active family
        // spell is now a candidate; the branches below still only accept damage, heal, absorb, summon, taunt,
        // interrupt and health-buff spells, so utility helpers are ignored as before.
        bool heal = info->HasEffect(SPELL_EFFECT_HEAL) || info->HasEffect(SPELL_EFFECT_HEAL_PCT) ||
                    info->HasAura(SPELL_AURA_PERIODIC_HEAL);
        bool absorb = info->HasAura(SPELL_AURA_SCHOOL_ABSORB);
        bool damage = info->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) || info->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE) ||
                      info->HasEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG) ||
                      info->HasAura(SPELL_AURA_PERIODIC_DAMAGE) || info->HasAura(SPELL_AURA_PERIODIC_LEECH);
        bool summon = info->HasEffect(SPELL_EFFECT_SUMMON) || info->HasEffect(SPELL_EFFECT_SUMMON_PET);
        bool periodic = info->HasAura(SPELL_AURA_PERIODIC_DAMAGE) || info->HasAura(SPELL_AURA_PERIODIC_HEAL);
        bool command = std::strncmp(info->SpellName[0], "Command:", 8) == 0;
        Unit* target = nullptr;
        int score = 0;
        if ((heal || absorb) && wounded->GetHealthPct() < (healer ? 88 : 60))
        {
            target = info->CheckExplicitTarget(bot, wounded) == SPELL_CAST_OK ? wounded : bot;
            if (target->GetHealthPct() >= (healer ? 88 : 60))
                continue;
            score = 300 - int(target->GetHealthPct());
        }
        else if (tank && enemy && enemy->GetVictim() && enemy->GetVictim() != bot &&
                 bot->IsFriendlyTo(enemy->GetVictim()) &&
                 (info->HasEffect(SPELL_EFFECT_ATTACK_ME) || info->HasAura(SPELL_AURA_MOD_TAUNT)))
        {
            target = enemy;
            score = 210;
        }
        else if (enemy && enemy->IsNonMeleeSpellCast(false) && info->HasEffect(SPELL_EFFECT_INTERRUPT_CAST))
        {
            target = enemy;
            score = 200;
        }
        else if (summon && !bot->IsInCombat())
        {
            bool necro = IsBotNecromancer(bot);
            if ((!necro && !bot->GetGuardianPet()) || (necro && AscensionNecromancer::State(bot).minions.empty()))
            {
                target = bot;
                score = spec == 35 ? 150 : 100;
            }
        }
        else if (bot->IsInCombat() && enemy && (damage || command))
        {
            target = enemy;
            score = command ? 110 : periodic ? 100 : 60;
            if (healer && wounded->GetHealthPct() < 90)
                continue;
            if (info->RecoveryTime || info->CategoryRecoveryTime)
                score += 15;
        }
        else if (info->IsPositive() && info->HasAura(SPELL_AURA_MOD_INCREASE_HEALTH) && !bot->IsInCombat())
        {
            target = bot;
            score = 50;
        }
        if (!target || ((periodic || absorb || info->IsPositive()) && target->HasAura(spell, bot->GetGUID())))
            continue;
        candidates.push_back({spell, target, score});
    }
    std::sort(candidates.begin(), candidates.end(), [](Candidate const& left, Candidate const& right)
              { return left.Score == right.Score ? left.Spell < right.Spell : left.Score > right.Score; });
    if (candidates.empty())
        ++actNoCandidate;
    time_t const nowTime = time(nullptr);
    for (auto const& candidate : candidates)
    {
        uint32 const backoffKey = bot->GetGUID().GetCounter() ^ (candidate.Spell * 2654435761u);
        SpellInfo const* candidateInfo = sSpellMgr->GetSpellInfo(candidate.Spell);
        bool const summonLike = candidateInfo && (candidateInfo->HasEffect(SPELL_EFFECT_SUMMON) || candidateInfo->HasEffect(SPELL_EFFECT_SUMMON_PET));
        if (summonLike)
        {
            // A summon that just failed (no capacity / resource) is retried every tick otherwise; wait a few seconds.
            std::lock_guard<std::mutex> lock(castMutex);
            auto retry = spellBackoff.find(backoffKey);
            if (retry != spellBackoff.end() && retry->second > nowTime)
            {
                ++castCounts[{static_cast<uint8>(bot->getClass()), candidate.Spell}].Suppressed;
                continue;
            }
        }
        if (bot->IsMounted() && candidate.Score >= 60)
        {
            // A mounted character cannot cast (SPELL_FAILED_NOT_MOUNTED). Measured: every failed Necromancer summon
            // was this. Get off the mount first, as a player would, and cast on the next tick.
            bot->Dismount();
            bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
            return true;
        }
        if (summonLike && bot->isMoving() && !bot->IsInCombat() && candidateInfo->CalcCastTime(bot) > 0 &&
            StopForCast(ai, 3))
            return true;  // walking bots cannot cast; stand still for a moment and summon on the next tick
        bool const casting = ai->CanCastSpell(candidate.Spell, candidate.Target);
        if (!casting && candidateInfo)
        {
            // Record why: the same check the bot AI uses, without the movement/range allowances.
            Spell probe(bot, candidateInfo, TRIGGERED_IGNORE_POWER_AND_REAGENT_COST);
            probe.m_targets.SetUnitTarget(candidate.Target);
            int const code = static_cast<int>(probe.CheckCast(true));
            std::lock_guard<std::mutex> lock(castMutex);
            ++castCounts[{static_cast<uint8>(bot->getClass()), candidate.Spell}].FailCodes[code];
        }
        if (!casting && summonLike)
        {
            std::lock_guard<std::mutex> lock(castMutex);
            if (spellBackoff.size() > 20000)
                spellBackoff.clear();
            spellBackoff[backoffKey] = nowTime + 8;
        }
        {
            std::lock_guard<std::mutex> lock(castMutex);
            auto& count = castCounts[{static_cast<uint8>(bot->getClass()), candidate.Spell}];
            if (!casting && candidateInfo &&
                candidateInfo->CalcPowerCost(bot, candidateInfo->GetSchoolMask()) > static_cast<int32>(bot->GetPower(Powers(candidateInfo->PowerType))))
                ++count.NoPower;
            if (count.Name.empty())
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(candidate.Spell))
                    count.Name = info->SpellName[0];
            ++count.Attempts;
            if (casting)
                ++count.Casts;
        }
        if (casting && ai->CastSpell(candidate.Spell, candidate.Target))
            return true;
    }
    return false;
}
}  // namespace Advai::Ascension
