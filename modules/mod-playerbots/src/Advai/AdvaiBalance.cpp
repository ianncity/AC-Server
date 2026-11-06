#include "GroupMgr.h"
#include "Group.h"
/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 *
 * Balance telemetry and spell smoke-test harness (GM console only).
 *
 *   .ascbench metrics on [minLevel]   start counting PvE damage/healing/deaths of bots at >= minLevel (default 80)
 *   .ascbench metrics off|reset|dump  stop / clear / print a per class+spec table and write a CSV
 *   .ascbench spells <class> [familyIds]  cast every known active spell of one online bot on a training dummy
 *   .ascbench spells stop|status
 *
 * Metrics are disabled by default: the hooks cost one relaxed atomic load while off.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>

#include "AdvaiMovement.h"
#include "AdvaiAscensionProgression.h"
#include "AiFactory.h"
#include "AscensionTalentSourceSpells.h"
#include "CapstoneTalents.h"
#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotMgr.h"
#include "RandomPlayerbotMgr.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"

uint32 GetDracthyrDragonDisplay(Player const* player);  // mod-expanded-races

using namespace Acore::ChatCommands;

namespace Advai::Balance
{
namespace
{
constexpr uint32 DummyEntry = 31146;  // Heroic Training Dummy: hostile, level 83, effectively unkillable
constexpr char const* ClassNames[] = {"?", "Warrior", "Paladin", "Hunter", "Rogue", "Priest", "DeathKnight",
                                      "Shaman", "Mage", "Warlock", "?", "Druid", "Cultist", "Necromancer"};

std::atomic<bool> metricsOn{false};
std::atomic<uint32> metricsMinLevel{80};
std::atomic<int64> metricsStarted{0};

struct Cell
{
    uint64 damage = 0, petDamage = 0, healing = 0, activeSeconds = 0, deaths = 0, events = 0, ilvlSum = 0;
    std::set<uint32> bots;
};
struct Who
{
    uint8 cls = 0, tab = 0, band = 0;
    int64 refreshed = 0, lastSecond = 0;
    uint64 active = 0;
};
std::mutex metricsMutex;
std::array<std::array<std::array<Cell, 4>, 14>, 5> cells;  // [level band][class][spec tab]
std::unordered_map<uint32, Who> whoMap;

int64 NowSec() { return static_cast<int64>(GameTime::GetGameTime().count()); }

uint32 EquippedItemLevel(Player* player)
{
    uint32 total = 0, count = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_BODY || slot == EQUIPMENT_SLOT_TABARD)
            continue;
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            total += item->GetTemplate()->ItemLevel;
            ++count;
        }
    }
    return count ? total / count : 0;
}

Player* OwnerBot(Unit* unit)
{
    if (!unit)
        return nullptr;
    Player* player = unit->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!player || !player->IsInWorld() || !sRandomPlayerbotMgr.IsRandomBot(player))
        return nullptr;
    return player;
}

Who& Lookup(Player* player, int64 now)
{
    Who& who = whoMap[player->GetGUID().GetCounter()];
    if (!who.cls || now - who.refreshed > 120)
    {
        who.cls = player->getClass();
        who.tab = std::min<uint8>(AiFactory::GetPlayerSpecTab(player), 3);
        who.band = static_cast<uint8>(std::min<uint32>(player->GetLevel() / 20, 4));
        who.refreshed = now;
    }
    return who;
}

void Account(Unit* source, uint64 amount, bool healing)
{
    Player* player = OwnerBot(source);
    if (!player || player->GetLevel() < metricsMinLevel.load(std::memory_order_relaxed))
        return;
    int64 const now = NowSec();
    std::lock_guard<std::mutex> lock(metricsMutex);
    Who& who = Lookup(player, now);
    if (who.cls >= 14)
        return;
    Cell& cell = cells[who.band][who.cls][who.tab];
    if (who.lastSecond != now)
    {
        who.lastSecond = now;
        ++who.active;
        ++cell.activeSeconds;
    }
    if (cell.bots.insert(player->GetGUID().GetCounter()).second)
        cell.ilvlSum += EquippedItemLevel(player);
    ++cell.events;
    if (healing)
        cell.healing += amount;
    else
    {
        cell.damage += amount;
        if (source != player)
            cell.petDamage += amount;
    }
}

// ---- spell smoke test -------------------------------------------------------------------------------------------
struct ProbeResult
{
    uint32 spell = 0;
    std::string name;
    int cast = 0;
    uint64 damage = 0, healing = 0;
    int targetAuras = 0, selfAuras = 0, summons = 0;
    int powerSpent = 0;
    std::string verdict;
};
struct Probe
{
    bool active = false;
    ObjectGuid bot, dummy;
    std::vector<uint32> spells;
    size_t index = 0;
    int32 timer = 0;
    std::vector<ProbeResult> results;
    std::set<uint32> baselineAuras;
    std::set<ObjectGuid> baselineControlled;
    ObjectGuid petBefore;
    std::vector<uint32> tempLearned;  // spells taught only for the duration of a capstone probe
    uint32 startPower = 0;
    Powers powerType = POWER_MANA;
    std::string label;
    uint8 cls = 0;
    int32 window = 2500;
};
Probe probe;
std::atomic<uint64> probeBot{0};
std::atomic<uint64> probeDamage{0}, probeHealing{0};

void ProbeAccount(Unit* source, uint64 amount, bool healing)
{
    uint64 const bot = probeBot.load(std::memory_order_relaxed);
    if (!bot || !source)
        return;
    Player* owner = source->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!owner || owner->GetGUID().GetRawValue() != bot)
        return;
    (healing ? probeHealing : probeDamage).fetch_add(amount, std::memory_order_relaxed);
}

class AdvaiBalanceUnitScript : public UnitScript
{
public:
    AdvaiBalanceUnitScript()
        : UnitScript("AdvaiBalanceUnitScript", true, {UNITHOOK_ON_DAMAGE, UNITHOOK_ON_HEAL, UNITHOOK_ON_UNIT_DEATH})
    {
    }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!damage || !attacker || !victim || attacker == victim)
            return;
        try
        {
            if (probeBot.load(std::memory_order_relaxed))
                ProbeAccount(attacker, damage, false);
            if (metricsOn.load(std::memory_order_relaxed) && victim->IsCreature() && !victim->IsControlledByPlayer())
                Account(attacker, damage, false);
        }
        catch (...)
        {
        }
    }

    void OnHeal(Unit* healer, Unit* /*receiver*/, uint32& gain) override
    {
        if (!gain || !healer)
            return;
        try
        {
            if (probeBot.load(std::memory_order_relaxed))
                ProbeAccount(healer, gain, true);
            if (metricsOn.load(std::memory_order_relaxed))
                Account(healer, gain, true);
        }
        catch (...)
        {
        }
    }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        if (!metricsOn.load(std::memory_order_relaxed) || !unit || !unit->IsPlayer())
            return;
        try
        {
            Player* player = unit->ToPlayer();
            if (!player || player->GetLevel() < metricsMinLevel.load(std::memory_order_relaxed) ||
                !sRandomPlayerbotMgr.IsRandomBot(player))
                return;
            std::lock_guard<std::mutex> lock(metricsMutex);
            Who& who = Lookup(player, NowSec());
            if (who.cls < 14)
                ++cells[who.band][who.cls][who.tab].deaths;
        }
        catch (...)
        {
        }
    }
};

std::string Describe(ProbeResult const& r)
{
    std::ostringstream out;
    out << r.spell << " " << r.name << " cast=" << (r.cast == 255 ? "OK" : "FAIL" + std::to_string(r.cast))
        << " dmg=" << r.damage << " heal=" << r.healing << " tAuras=" << r.targetAuras << " sAuras=" << r.selfAuras
        << " summons=" << r.summons << " power=" << r.powerSpent << " => " << r.verdict;
    return out.str();
}

void FinishProbe(std::string const& reason)
{
    probeBot.store(0);
    if (Player* tester = ObjectAccessor::FindPlayer(probe.bot))
        for (uint32 spell : probe.tempLearned)
            tester->removeSpell(spell, SPEC_MASK_ALL, false);
    probe.tempLearned.clear();
    std::ostringstream path;
    path << "advai-balance-spells-" << probe.label << "-" << time(nullptr) << ".txt";
    std::ofstream file(path.str());
    uint32 ok = 0, fail = 0, silent = 0;
    for (ProbeResult const& r : probe.results)
    {
        file << Describe(r) << "\n";
        if (r.cast != 255)
            ++fail;
        else if (r.verdict == "NO_OBSERVABLE_EFFECT")
            ++silent;
        else
            ++ok;
    }
    LOG_INFO("playerbots", "ascbench spells {} finished ({}): {} spells, {} effective, {} cast-failed, {} silent. Report: {}",
             probe.label, reason, probe.results.size(), ok, fail, silent, path.str());
    probe.active = false;
}

bool CastsOnEnemy(SpellInfo const* info)
{
    for (SpellEffectInfo const& effect : info->Effects)
        if (effect.IsEffect() && (effect.TargetA.GetCheckType() == TARGET_CHECK_ENEMY ||
                                  effect.TargetB.GetCheckType() == TARGET_CHECK_ENEMY))
            return true;
    return !info->IsPositive() || info->HasEffect(SPELL_EFFECT_SCHOOL_DAMAGE) ||
        info->HasEffect(SPELL_EFFECT_WEAPON_DAMAGE) || info->HasEffect(SPELL_EFFECT_WEAPON_PERCENT_DAMAGE);
}

void StepProbe()
{
    Player* bot = ObjectAccessor::FindPlayer(probe.bot);
    if (!bot || !bot->IsInWorld() || !bot->IsAlive())
    {
        FinishProbe("bot unavailable");
        return;
    }
    PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(bot);
    if (ai)
        ai->SetNextCheckDelay(6000);

    Creature* dummy = ObjectAccessor::GetCreature(*bot, probe.dummy);
    // Finalize the previous spell.
    if (probe.index > 0 && !probe.results.empty() && probe.results.back().verdict.empty())
    {
        ProbeResult& r = probe.results.back();
        r.damage = probeDamage.exchange(0);
        r.healing = probeHealing.exchange(0);
        if (dummy && dummy->GetMaxHealth() > dummy->GetHealth())
            r.damage = std::max<uint64>(r.damage, dummy->GetMaxHealth() - dummy->GetHealth());
        if (dummy)
            r.targetAuras = static_cast<int>(dummy->GetAppliedAuras().size());
        for (auto const& [id, app] : bot->GetAppliedAuras())
            if (!probe.baselineAuras.count(id))
                ++r.selfAuras;
        for (Unit* unit : bot->m_Controlled)
            if (unit && !probe.baselineControlled.count(unit->GetGUID()))
                ++r.summons;
        if (Pet* pet = bot->GetPet(); pet && pet->GetGUID() != probe.petBefore)
        {
            ++r.summons;
            r.name += " [new pet entry " + std::to_string(pet->GetEntry()) + "]";
        }
        r.powerSpent = static_cast<int>(probe.startPower) - static_cast<int>(bot->GetPower(probe.powerType));
        bool const effect = r.damage || r.healing || r.targetAuras || r.selfAuras || r.summons;
        r.verdict = r.cast != 255 ? "CAST_FAILED" : effect ? "OK" : "NO_OBSERVABLE_EFFECT";
        LOG_DEBUG("playerbots", "ascbench {}", Describe(r));
        // Cleanup before the next spell.
        if (dummy)
            dummy->RemoveAllAuras();
        std::vector<uint32> addedAuras;
        for (auto const& [id, app] : bot->GetAppliedAuras())
            if (!probe.baselineAuras.count(id))
                addedAuras.push_back(id);
        for (uint32 id : addedAuras)
            bot->RemoveAurasDueToSpell(id);
        std::vector<Unit*> controlled(bot->m_Controlled.begin(), bot->m_Controlled.end());
        for (Unit* unit : controlled)
            if (unit && !probe.baselineControlled.count(unit->GetGUID()))
                if (Creature* creature = unit->ToCreature())
                    if (TempSummon* summon = creature->ToTempSummon())
                        summon->UnSummon();
    }

    if (probe.index >= probe.spells.size())
    {
        if (dummy)
            dummy->DespawnOrUnsummon();
        FinishProbe("complete");
        return;
    }

    if (!dummy || !dummy->IsAlive())
    {
        Position pos = bot->GetNearPosition(5.0f, 0.0f);  // straight ahead, in line of sight and inside melee-ish range
        TempSummon* summon = bot->GetMap()->SummonCreature(DummyEntry, pos, nullptr, 10 * 60 * 1000);
        if (!summon)
        {
            FinishProbe("cannot summon dummy");
            return;
        }
        probe.dummy = summon->GetGUID();
        dummy = summon;
        // The ordinary training-dummy AI zeros DamageTaken before telemetry and health changes.
        auto passive = std::make_unique<NullCreatureAI>(dummy);
        if (!dummy->AIM_Initialize(passive.get()))
        {
            dummy->DespawnOrUnsummon();
            FinishProbe("cannot initialize damageable probe target");
            return;
        }
        passive.release();
        dummy->SetPhaseMask(bot->GetPhaseMask(), true);
        dummy->SetFaction(14);
        dummy->SetLevel(bot->GetLevel());
        dummy->SetMaxHealth(10000000);
    }
    dummy->SetFullHealth();
    bot->GetMotionMaster()->Clear();
    bot->StopMoving();
    bot->SetFullHealth();
    if (probe.powerType != POWER_RAGE && probe.powerType != POWER_RUNIC_POWER)
        bot->SetPower(probe.powerType, bot->GetMaxPower(probe.powerType));
    bot->RemoveAllSpellCooldown();
    bot->SetSelection(dummy->GetGUID());
    bot->SetFacingToObject(dummy);
    dummy->SetFacingToObject(bot);
    probe.startPower = bot->GetPower(probe.powerType);
    probeDamage.store(0);
    probeHealing.store(0);

    uint32 const spellId = probe.spells[probe.index++];
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    ProbeResult result;
    result.spell = spellId;
    result.name = info ? info->SpellName[0] ? info->SpellName[0] : "" : "";
    if (!info)
    {
        result.cast = -1;
        result.verdict = "NO_SPELLINFO";
        probe.results.push_back(result);
        probe.timer = 100;
        return;
    }
    // Everything except the cast-time/GCD/cost machinery stays enabled: the point is to run the spell's own effect
    // and script code, not to bypass legitimate target and state validation.
    TriggerCastFlags const flags = TriggerCastFlags(TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_SPELL_AND_CATEGORY_CD |
                                                    TRIGGERED_IGNORE_POWER_AND_REAGENT_COST | TRIGGERED_IGNORE_CAST_ITEM |
                                                    TRIGGERED_IGNORE_CAST_IN_PROGRESS | TRIGGERED_IGNORE_AURA_INTERRUPT_FLAGS |
                                                    TRIGGERED_IGNORE_CASTER_MOUNTED_OR_ON_VEHICLE |
                                                    TRIGGERED_IGNORE_CASTER_AURASTATE | TRIGGERED_IGNORE_CASTER_AURAS |
                                                    TRIGGERED_IGNORE_SHAPESHIFT | TRIGGERED_IGNORE_EQUIPPED_ITEM_REQUIREMENT |
                                                    TRIGGERED_IGNORE_COMBO_POINTS);
    Unit* target = CastsOnEnemy(info) ? static_cast<Unit*>(dummy) : static_cast<Unit*>(bot);
    probe.petBefore = bot->GetPetGUID();
    bot->Dismount();
    bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
    if (!bot->IsStandState())
        bot->SetStandState(UNIT_STAND_STATE_STAND);
    bot->SetSelection(dummy->GetGUID());
    try
    {
        SpellCastTargets targets;
        targets.SetUnitTarget(target);
        if (info->Targets & TARGET_FLAG_DEST_LOCATION)
            targets.SetDst(*target);
        if (info->Targets & TARGET_FLAG_SOURCE_LOCATION)
            targets.SetSrc(*bot);
        result.cast = static_cast<int>(bot->CastSpell(targets, info, nullptr, flags));
    }
    catch (std::exception const& error)
    {
        result.cast = -2;
        result.verdict = std::string("EXCEPTION ") + error.what();
    }
    catch (...)
    {
        result.cast = -2;
        result.verdict = "EXCEPTION";
    }
    probe.results.push_back(result);
    probe.timer = probe.window;  // observation window for periodic/channelled effects
}

class AdvaiBalanceWorldScript : public WorldScript
{
public:
    AdvaiBalanceWorldScript() : WorldScript("AdvaiBalanceWorldScript", {WORLDHOOK_ON_UPDATE}) {}
    void OnUpdate(uint32 diff) override
    {
        if (!probe.active)
            return;
        probe.timer -= static_cast<int32>(diff);
        if (probe.timer > 0)
            return;
        try
        {
            StepProbe();
        }
        catch (std::exception const& error)
        {
            LOG_ERROR("playerbots", "ascbench probe aborted: {}", error.what());
            FinishProbe("exception");
        }
        catch (...)
        {
            FinishProbe("exception");
        }
    }
};

void Dump(ChatHandler* handler)
{
    std::lock_guard<std::mutex> lock(metricsMutex);
    int64 const elapsed = std::max<int64>(1, NowSec() - metricsStarted.load());
    std::ostringstream csv;
    csv << "band,class,tab,bots,active_sec,dps,hps,pet_pct,deaths,deaths_per_bot,events\n";
    handler->PSendSysMessage("ascbench metrics: window {}s, minLevel {}", elapsed, metricsMinLevel.load());
    for (size_t band = 0; band < 5; ++band)
    for (size_t cls = 0; cls < 14; ++cls)
        for (size_t tab = 0; tab < 4; ++tab)
        {
            Cell const& c = cells[band][cls][tab];
            if (!c.events && !c.deaths)
                continue;
            double const active = std::max<double>(1.0, double(c.activeSeconds));
            double const dps = double(c.damage) / active, hps = double(c.healing) / active;
            double const pet = c.damage ? 100.0 * double(c.petDamage) / double(c.damage) : 0.0;
            size_t const bots = c.bots.size();
            double const dpb = bots ? double(c.deaths) / double(bots) : 0.0;
            char line[256];
            double const ilvl = bots ? double(c.ilvlSum) / double(bots) : 0.0;
            snprintf(line, sizeof(line), "L%-2zu %-12s tab%zu bots=%3zu ilvl=%5.1f active=%7llu dps=%7.1f hps=%7.1f pet%%=%4.1f deaths=%llu (%.2f/bot)",
                     band * 20, ClassNames[cls], tab, bots, ilvl, (unsigned long long) c.activeSeconds, dps, hps, pet,
                     (unsigned long long) c.deaths, dpb);
            handler->SendSysMessage(line);
            csv << band * 20 << "," << ClassNames[cls] << "," << tab << "," << bots << "," << ilvl << "," << c.activeSeconds << "," << dps << ","
                << hps << "," << pet << "," << c.deaths << "," << dpb << "," << c.events << "\n";
        }
    std::ofstream file("advai-balance-metrics-" + std::to_string(time(nullptr)) + ".csv");
    file << csv.str();
}

bool StartProbe(ChatHandler* handler, uint8 cls, std::set<uint32> const& families, bool catalog, int32 window, uint32 wantLevel, bool capstone)
{
    if (probe.active)
    {
        handler->SendSysMessage("A spell probe is already running; use 'ascbench spells stop'.");
        return false;
    }
    Player* best = nullptr;
    for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
        if (player && player->IsInWorld() && player->IsAlive() && player->getClass() == cls &&
            sRandomPlayerbotMgr.IsRandomBot(player) && !player->IsInCombat() && !player->GetMap()->IsDungeon() &&
            !player->IsInFlight() && !player->IsBeingTeleported() &&
            (!best || (wantLevel ? std::abs(int(player->GetLevel()) - int(wantLevel)) < std::abs(int(best->GetLevel()) - int(wantLevel))
                                 : player->GetLevel() > best->GetLevel())))
            best = player;
    if (!best)
    {
        handler->SendSysMessage("No idle online bot of that class.");
        return false;
    }
    probe = Probe();
    probe.cls = cls;
    probe.bot = best->GetGUID();
    probe.label = std::string(ClassNames[cls < 14 ? cls : 0]) + "-L" + std::to_string(best->GetLevel());
    probe.powerType = best->getPowerType();
    probe.window = std::clamp<int32>(window, 500, 10000);
    std::set<uint32> sorted;
    if (capstone)
    {
        // Teach the class's capstone talent (and, for Warlocks, the stock demon summons Supremacy replaces) temporarily so
        // its own scripts see a character that really has the talent, then probe everything it makes castable.
        auto teach = [&](uint32 spell)
        {
            if (!best->HasSpell(spell) && sSpellMgr->GetSpellInfo(spell))
            {
                best->learnSpell(spell, false);
                probe.tempLearned.push_back(spell);
            }
        };
        if (cls == 9)
            for (uint32 demon : {688u, 697u, 712u, 691u, 30146u, 1122u, 18540u})
                teach(demon);
        for (CapstoneTalents::Entry const& entry : CapstoneTalents::Entries)
            if (entry.ClassId == cls)
            {
                teach(entry.SpellId);
                sorted.insert(entry.SpellId);
            }
        for (auto const& [spell, entry] : best->GetSpellMap())
            if (spell >= 991000 && spell <= 991100)
                if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spell); info && !info->IsPassive())
                    sorted.insert(spell);
    }
    else if (catalog)
    {
        // Full authored catalog of the custom class: trainer spells plus the highest rank of every talent in its trees.
        uint32 const trainer = cls == 12 ? 98000 : cls == 13 ? 98001 : 0;
        if (trainer)
            if (QueryResult result = WorldDatabase.Query("SELECT SpellId FROM trainer_spell WHERE TrainerId = {}", trainer))
                do
                    sorted.insert((*result)[0].Get<uint32>());
                while (result->NextRow());
        uint32 const firstTree = cls == 12 ? 0 : cls == 13 ? 4 : 99;
        if (firstTree < 7)
            for (uint32 tree = firstTree; tree < firstTree + 3; ++tree)
                for (uint32 node = 0; node < AscensionTalentSources::TalentNodeCountsByTree[tree]; ++node)
                {
                    uint32 const talentId = AscensionTalentSources::TalentIdsByTree[tree][node];
                    if (!talentId)
                        continue;
                    uint32 const index = talentId - AscensionTalentSources::FirstTalentId;
                    for (int rank = AscensionTalentSources::MaxTalentRanks - 1; rank >= 0; --rank)
                        if (uint32 spell = AscensionTalentSources::TalentSpellIds[index][rank])
                        {
                            sorted.insert(spell);
                            break;
                        }
                }
        // Every class carries a capstone talent; include the ones belonging to this class.
        for (CapstoneTalents::Entry const& entry : CapstoneTalents::Entries)
            if (entry.ClassId == cls)
                sorted.insert(entry.SpellId);
        for (auto it = sorted.begin(); it != sorted.end();)
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(*it);
            it = (!info || info->IsPassive() || (info->IsPositive() && info->Effects[0].Effect == SPELL_EFFECT_APPLY_AURA &&
                                                   info->Effects[0].ApplyAuraName == SPELL_AURA_DUMMY && !info->GetDuration()))
                ? sorted.erase(it) : std::next(it);
        }
    }
    else
    for (auto const& [spell, entry] : best->GetSpellMap())
    {
        if (entry->State == PLAYERSPELL_REMOVED || !entry->Active)
            continue;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell);
        if (!info || info->IsPassive())
            continue;
        if (families.empty())
        {
            // Default scope: the class's own spell book. Custom classes use ids >= 300000 plus their donor family.
            static std::map<uint8, uint32> const donorFamily = {{1, 4},  {2, 10}, {3, 9}, {4, 8},  {5, 6},  {6, 15},
                                                                {7, 11}, {8, 3},  {9, 5}, {11, 7}, {12, 6}, {13, 5}};
            auto const family = donorFamily.find(cls);
            bool const own = family != donorFamily.end() && info->SpellFamilyName == family->second;
            if (!own && !(cls >= 12 && spell >= 300000))
                continue;
        }
        else if (!families.count(info->SpellFamilyName) && !(families.count(999) && spell >= 300000))
            continue;
        if (uint32 next = sSpellMgr->GetNextSpellInChain(spell); next && best->HasSpell(next))
            continue;  // only the highest known rank
        sorted.insert(spell);
    }
    probe.spells.assign(sorted.begin(), sorted.end());
    probe.spells.insert(probe.spells.begin(), 133); // Fireball rank one validates damage measurement independently.
    for (auto const& [id, app] : best->GetAppliedAuras())
        probe.baselineAuras.insert(id);
    for (Unit* unit : best->m_Controlled)
        if (unit)
            probe.baselineControlled.insert(unit->GetGUID());
    probe.timer = 0;
    probe.active = true;
    probeBot.store(best->GetGUID().GetRawValue());
    handler->PSendSysMessage("Probing {} spells on {} ({}, level {}, spec tab {}). Report is written when done.",
                             probe.spells.size(), best->GetName(), ClassNames[cls], best->GetLevel(),
                             AiFactory::GetPlayerSpecTab(best));
    return true;
}

class advai_balance_commandscript : public CommandScript
{
public:
    advai_balance_commandscript() : CommandScript("advai_balance_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable table = {{"ascbench", Handle, SEC_GAMEMASTER, Console::Yes}};
        return table;
    }

    static bool Handle(ChatHandler* handler, char const* args)
    {
        std::istringstream in(args ? args : "");
        std::string verb, arg;
        in >> verb >> arg;
        if (verb == "metrics")
        {
            if (arg == "on")
            {
                uint32 minLevel = 80;
                in >> minLevel;
                metricsMinLevel.store(minLevel);
                metricsStarted.store(NowSec());
                metricsOn.store(true);
                handler->PSendSysMessage("ascbench metrics ON (bots level >= {})", minLevel);
            }
            else if (arg == "off")
            {
                metricsOn.store(false);
                handler->SendSysMessage("ascbench metrics OFF");
            }
            else if (arg == "reset")
            {
                std::lock_guard<std::mutex> lock(metricsMutex);
                for (auto& level : cells)
                    for (auto& row : level)
                        for (Cell& cell : row)
                            cell = Cell();
                whoMap.clear();
                metricsStarted.store(NowSec());
                handler->SendSysMessage("ascbench metrics reset");
            }
            else if (arg == "dump")
                Dump(handler);
            else
                handler->SendSysMessage("usage: ascbench metrics on [minLevel]|off|reset|dump");
            return true;
        }
        if (verb == "say")
        {
            // Usage: ascbench say <player> <languageId> <text...>  - send ordinary /say as that player with the given language id.
            Player* who = ObjectAccessor::FindPlayerByName(arg);
            uint32 languageId = 0;
            in >> languageId;
            std::string text;
            std::getline(in >> std::ws, text);
            if (!who || text.empty())
            {
                handler->SendSysMessage("usage: ascbench say <player> <languageId> <text>");
                return true;
            }
            WorldPacket packet(CMSG_MESSAGECHAT);
            packet << uint32(CHAT_MSG_SAY) << uint32(languageId) << text;
            who->GetSession()->HandleMessagechatOpcode(packet);
            handler->PSendSysMessage("{} said '{}' with language {}; see Server.log for CHAT_LANGUAGE_FIX.", who->GetName(), text, languageId);
            return true;
        }
        if (verb == "ritual")
        {
            // Usage: ascbench ritual <warlock> <target>  - start Ritual of Summoning as the warlock on a groupmate.
            Player* lock = ObjectAccessor::FindPlayerByName(arg);
            std::string targetName;
            in >> targetName;
            Player* target = ObjectAccessor::FindPlayerByName(targetName);
            if (!lock || !target)
            {
                handler->SendSysMessage("usage: ascbench ritual <warlock> <target>");
                return true;
            }
            // Optional helpers: ascbench ritual <warlock> <target> [helper ...] groups everyone with the warlock first.
            std::vector<Player*> members{target};
            for (std::string helperName; in >> helperName;)
                if (Player* helper = ObjectAccessor::FindPlayerByName(helperName))
                    members.push_back(helper);
            Group* testGroup = lock->GetGroup();
            if (!testGroup)
            {
                testGroup = new Group;
                if (!testGroup->Create(lock))
                {
                    delete testGroup;
                    testGroup = nullptr;
                }
                else
                    sGroupMgr->AddGroup(testGroup);
            }
            for (Player* member : members)
                if (testGroup && !member->GetGroup())
                    testGroup->AddMember(member);
            lock->Dismount();
            lock->SetSelection(target->GetGUID());
            SpellCastResult const result = lock->CastSpell(target, 698, TRIGGERED_FULL_MASK);
            LOG_INFO("playerbots", "ritual {} -> {} cast result {}", lock->GetName(), target->GetName(),
                     static_cast<int>(result));
            handler->SendSysMessage("ritual started; see Playerbots.log");
            return true;
        }
        if (verb == "pettest")
        {
            // Cast one summon spell on a named bot and log what pet/minions exist right after and 3 s later.
            // Usage: ascbench pettest <name> <spellId> [talentSpell demonSpell ...]  (extra spells are taught first)
            Player* who = ObjectAccessor::FindPlayerByName(arg);
            uint32 spellId = 0;
            in >> spellId;
            if (!who || !spellId)
            {
                handler->SendSysMessage("usage: ascbench pettest <name> <spellId> [spells to learn first]");
                return true;
            }
            uint32 extra;
            while (in >> extra)
                if (!who->HasSpell(extra) && sSpellMgr->GetSpellInfo(extra))
                    who->learnSpell(extra, false);
            if (PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(who))
                ai->SetNextCheckDelay(8000);
            who->Dismount();
            auto report = [](Player* p, char const* when)
            {
                Pet* pet = p->GetPet();
                std::string minions;
                for (Unit* unit : p->m_Controlled)
                    if (unit)
                        minions += std::to_string(unit->GetEntry()) + " ";
                LOG_INFO("playerbots", "pettest {} {}: pet={} petGuid={} alive={} controlled=[{}] hasTalent={} knows688={} knows697={}",
                         p->GetName(), when, pet ? pet->GetEntry() : 0, p->GetPetGUID().ToString(), pet ? pet->IsAlive() : false,
                         minions, p->HasActiveSpell(990100), p->HasActiveSpell(688), p->HasActiveSpell(697));
            };
            LOG_INFO("playerbots", "pettest {} cast redirects: 688->{} 691->{} 697->{} 712->{} 30146->{} 18540->{}",
                     who->GetName(), who->GetCastReplacement(688), who->GetCastReplacement(691),
                     who->GetCastReplacement(697), who->GetCastReplacement(712), who->GetCastReplacement(30146),
                     who->GetCastReplacement(18540));
            report(who, "before");
            SpellCastResult const result = who->CastSpell(who, spellId, TRIGGERED_FULL_MASK);
            LOG_INFO("playerbots", "pettest {} cast result {}", who->GetName(), static_cast<int>(result));
            report(who, "immediately after");
            ObjectGuid const guid = who->GetGUID();
            if (PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(who))
                ai->AddTimedEvent([guid, report] { if (Player* p = ObjectAccessor::FindPlayer(guid)) report(p, "3s later"); }, 3000);
            handler->SendSysMessage("pettest started; see Playerbots.log");
            return true;
        }
        if (verb == "dragon")
        {
            // Reproduce the wardrobe's DRAGONARMOR step on a Dracthyr and re-read the display twice afterwards, to see
            // whether anything reverts it. Usage: ascbench dragon <name> <armor 36-73>
            Player* who = ObjectAccessor::FindPlayerByName(arg);
            uint32 armor = 0;
            in >> armor;
            if (!who || (who->getRace() != 12 && who->getRace() != 13) || armor >= 74)
            {
                handler->SendSysMessage("usage: ascbench dragon <online Dracthyr name> <armor 0-73>");
                return true;
            }
            auto report = [](Player* p, char const* when)
            {
                LOG_INFO("playerbots", "dragon-test {} {}: armorByte={} native={} display={} expected={}", p->GetName(), when,
                         p->GetByteValue(PLAYER_BYTES_2, PLAYER_BYTES_2_OFFSET_FACIAL_STYLE), p->GetNativeDisplayId(),
                         p->GetDisplayId(), GetDracthyrDragonDisplay(p));
            };
            report(who, "before");
            who->SetByteValue(PLAYER_BYTES_2, PLAYER_BYTES_2_OFFSET_FACIAL_STYLE, static_cast<uint8>(armor));
            uint32 const dragon = GetDracthyrDragonDisplay(who);
            if (dragon)
            {
                who->SetNativeDisplayId(dragon);
                who->SetDisplayId(dragon);
            }
            report(who, "immediately after");
            ObjectGuid const guid = who->GetGUID();
            if (PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(who))
            {
                ai->AddTimedEvent([guid, report] { if (Player* p = ObjectAccessor::FindPlayer(guid)) report(p, "2s later"); }, 2000);
                ai->AddTimedEvent([guid, report] { if (Player* p = ObjectAccessor::FindPlayer(guid)) report(p, "6s later"); }, 6000);
            }
            handler->PSendSysMessage("dragon test run; see Playerbots.log for 'dragon-test' lines");
            return true;
        }
        if (verb == "casts")
        {
            handler->SendSysMessage(Advai::Ascension::CastReport().c_str());
            if (arg == "reset")
                Advai::Ascension::ResetCastReport();
            return true;
        }
        if (verb == "why")
        {
            Player* who = ObjectAccessor::FindPlayerByName(arg);
            PlayerbotAI* ai = who ? PlayerbotsMgr::instance().GetPlayerbotAI(who) : nullptr;
            handler->SendSysMessage(ai ? Advai::Ascension::Explain(ai).c_str() : "bot not found");
            return true;
        }
        if (verb == "pick")
        {
            // Print the names of online random bots of a class/level band so they can be inspected with 'why'.
            uint32 cls = static_cast<uint32>(atoi(arg.c_str())), level = 0;
            in >> level;
            std::string names;
            uint32 n = 0;
            for (auto const& [guid, player] : ObjectAccessor::GetPlayers())
                if (player && player->getClass() == cls && sRandomPlayerbotMgr.IsRandomBot(player) &&
                    (!level || (player->GetLevel() >= level && player->GetLevel() <= level + 3)) && n++ < 12)
                    names += player->GetName() + "(" + std::to_string(player->GetLevel()) + ") ";
            handler->SendSysMessage(names.empty() ? "none" : names.c_str());
            return true;
        }
        if (verb == "moves")
        {
            handler->SendSysMessage(Advai::DescribeMovementStats(arg == "reset").c_str());
            return true;
        }
        if (verb == "login")
        {
            // Bring level-80 random-bot characters online (evenly per class) so a balance run has a population at
            // the same level. Usage: ascbench login80 [perClass=40]
            uint32 const loginLevel = static_cast<uint32>(atoi(arg.c_str()));
            uint32 perClass = 0, onlyClass = 0;
            in >> perClass >> onlyClass;
            if (!perClass)
                perClass = 40;
            if (!loginLevel)
            {
                handler->SendSysMessage("usage: ascbench login <level> [perClass=40] [classId]  (logs in characters of level..level+3)");
                return true;
            }
            std::ostringstream accounts;
            bool first = true;
            for (uint32 account : sPlayerbotAIConfig.RandomBotAccounts)
            {
                accounts << (first ? "" : ",") << account;
                first = false;
            }
            if (first)
            {
                handler->SendSysMessage("No random-bot accounts.");
                return true;
            }
            uint32 queued = 0;
            for (uint32 cls : {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13})
                if (!onlyClass || cls == onlyClass)
                if (QueryResult result = CharacterDatabase.Query(
                        "SELECT guid FROM characters WHERE level BETWEEN {} AND {} AND class = {} AND online = 0 AND account IN ({}) "
                        "ORDER BY RAND() LIMIT {}", loginLevel, loginLevel + 3, cls, accounts.str(), perClass))
                    do
                        if (sRandomPlayerbotMgr.ForceRandomBotLogin((*result)[0].Get<uint32>()))
                            ++queued;
                    while (result->NextRow());
            handler->PSendSysMessage("Queued {} bots of level {}-{} for login.", queued, loginLevel, loginLevel + 3);
            return true;
        }
        if (verb == "spells")
        {
            if (arg == "stop")
            {
                if (probe.active)
                    FinishProbe("stopped");
                return true;
            }
            if (arg == "status")
            {
                handler->PSendSysMessage("probe active={} step={}/{}", probe.active, probe.index, probe.spells.size());
                for (size_t i = probe.results.size() > 10 ? probe.results.size() - 10 : 0; i < probe.results.size(); ++i)
                    handler->SendSysMessage(Describe(probe.results[i]));
                return true;
            }
            uint32 cls = static_cast<uint32>(atoi(arg.c_str()));
            if (cls == 0 || cls > 13)
            {
                handler->SendSysMessage("usage: ascbench spells <classId> [spellFamily ... | 999=custom ids] | stop | status");
                return true;
            }
            std::set<uint32> families;
            std::string token;
            bool catalogMode = false, capstoneMode = false;
            int32 window = 2500;
            uint32 wantLevel = 0;
            while (in >> token)
            {
                if (token == "catalog")
                    catalogMode = true;
                else if (token == "capstone")
                    capstoneMode = true;
                else if (token.rfind("level=", 0) == 0)
                    wantLevel = static_cast<uint32>(atoi(token.c_str() + 6));
                else if (token.rfind("window=", 0) == 0)
                    window = atoi(token.c_str() + 7);
                else
                    families.insert(static_cast<uint32>(atoi(token.c_str())));
            }
            StartProbe(handler, static_cast<uint8>(cls), families, catalogMode, window, wantLevel, capstoneMode);
            return true;
        }
        handler->SendSysMessage("usage: ascbench metrics ... | ascbench spells ...");
        return true;
    }
};
}  // namespace

void AddAdvaiBalanceScripts()
{
    new AdvaiBalanceUnitScript();
    new AdvaiBalanceWorldScript();
    new advai_balance_commandscript();
}
}  // namespace Advai::Balance

void AddAdvaiBalanceScripts() { Advai::Balance::AddAdvaiBalanceScripts(); }
