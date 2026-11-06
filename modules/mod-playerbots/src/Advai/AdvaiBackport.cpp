/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiBackport.h"

#include <array>
#include <chrono>
#include <stdexcept>
#include <string_view>

#include "Ascension/AdvaiAscensionIdentity.h"
#include "AdvaiAscensionProgression.h"
#include "AdvaiMgr.h"
#include "AiObjectContext.h"
#include "CreatureScript.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "Event.h"
#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerScript.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaceMgr.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Trainer.h"

namespace Advai::Ascension
{
void ValidateContent();
}

void AddAdvaiAscensionScripts();

namespace Advai::Backport
{
std::vector<ClassAbility> const& Abilities()
{
    static std::vector<ClassAbility> const abilities = {{589, 1, ClassProfile::Cultist},
                                                        {8092, 10, ClassProfile::Cultist},
                                                        {15407, 20, ClassProfile::Cultist},
                                                        {34914, 50, ClassProfile::Cultist},
                                                        {15473, 40, ClassProfile::Cultist},
                                                        {8122, 14, ClassProfile::Cultist},
                                                        {47585, 60, ClassProfile::Cultist},
                                                        {15286, 30, ClassProfile::Cultist},
                                                        {BIND_TENTACLE, 19, ClassProfile::Cultist},
                                                        {BIND_FACELESS, 39, ClassProfile::Cultist},
                                                        {686, 1, ClassProfile::Necromancer},
                                                        {689, 14, ClassProfile::Necromancer},
                                                        {NECRO_FROSTBOLT, 4, ClassProfile::Necromancer},
                                                        {RAISE_INITIATE, 1, ClassProfile::Necromancer},
                                                        {BONE_SHIELD, 20, ClassProfile::Necromancer},
                                                        {CORPSE_EXPLOSION, 30, ClassProfile::Necromancer},
                                                        {DEATH_COIL, 40, ClassProfile::Necromancer},
                                                        {ICY_TOUCH, 10, ClassProfile::Necromancer},
                                                        {RAISE_MAGE, 20, ClassProfile::Necromancer},
                                                        {RAISE_ARCHER, 36, ClassProfile::Necromancer},
                                                        {RAISE_GOLEM, 50, ClassProfile::Necromancer}};
    return abilities;
}

ClassProfile Assign(uint8 classId)
{
    if (classId == CLASS_CULTIST)
        return ClassProfile::Cultist;
    if (classId == CLASS_NECROMANCER)
        return ClassProfile::Necromancer;
    return ClassProfile::Classic;
}

char const* Name(ClassProfile profile)
{
    switch (profile)
    {
        case ClassProfile::Cultist:
            return "Cultist";
        case ClassProfile::Necromancer:
            return "Necromancer";
        case ClassProfile::Classic:
            return "Classic";
        default:
            throw std::invalid_argument("ADVAI invalid class profile");
    }
}

bool AllowsSpell(ClassProfile profile, uint32 spell)
{
    if (spell < NECRO_FROSTBOLT || spell > BIND_FACELESS)
        return true;
    for (auto const& ability : Abilities())
        if (ability.Spell == spell)
            return ability.Profile == profile;
    return false;
}

bool IsDracthyr(uint8 race) { return race == 12 || race == 13; }

void Validate()
{
    if (!sPlayerbotAIConfig.AdvaiEnabled)
        return;
    Advai::Ascension::ValidateContent();
    if (!Ascension::Enabled())
        for (auto const& ability : Abilities())
            if (!sSpellMgr->GetSpellInfo(ability.Spell))
                throw std::runtime_error(
                    "ADVAI custom class spell catalog is missing; install the matching DBC bundle");
    if (!Ascension::Enabled())
    {
        for (uint32 entry : {95000, 95001, 95002, 95003, 95010, 95011})
        {
            if (!sObjectMgr->GetCreatureTemplate(entry) || !sObjectMgr->GetPetLevelInfo(entry, 1) ||
                !sObjectMgr->GetPetLevelInfo(entry, 80))
                throw std::runtime_error("ADVAI custom companion templates/level statistics are incomplete");
        }
        for (uint32 display : {200, 1421, 1420, 10971, 25547, 25677})
            if (!sCreatureDisplayInfoStore.LookupEntry(display) || !sObjectMgr->GetCreatureModelInfo(display))
                throw std::runtime_error("ADVAI companion display/model metadata is incomplete");
        for (uint32 entry : {95020, 95021, 95022, 95023})
        {
            auto* trainer = sObjectMgr->GetTrainer(entry);
            if (!trainer || trainer->GetSpells().empty())
                throw std::runtime_error("ADVAI class trainers are missing; apply the optional class world migration");
        }
    }
    if (!sPlayerbotAIConfig.AdvaiDracthyr)
        return;
    LOG_INFO("server.loading", "Validating Dracthyr bot name pools...");
    auto names = CharacterDatabase.Query(
        "SELECT COALESCE(SUM(gender=18),0),COALESCE(SUM(gender=19),0) FROM playerbots_names");
    if (!names || names->GetFieldCount() != 2)
        throw std::runtime_error("ADVAI Dracthyr names are missing; apply the characters name migration");
    Field* nameCounts = names->Fetch();
    if (!nameCounts || !nameCounts[0].Get<uint64>() || !nameCounts[1].Get<uint64>())
        throw std::runtime_error("ADVAI Dracthyr names are missing; apply the characters name migration");
    LOG_INFO("server.loading", "Dracthyr bot name pools validated.");
    constexpr std::array<uint8, 9> dracthyrClasses = {
        CLASS_WARRIOR, CLASS_HUNTER, CLASS_ROGUE, CLASS_PRIEST, CLASS_DEATH_KNIGHT,
        CLASS_MAGE, CLASS_WARLOCK, CLASS_CULTIST, CLASS_NECROMANCER};
    if (sObjectMgr->GetPlayerInfo(RACE_ORC, CLASS_CULTIST))
        throw std::runtime_error("ADVAI race rules are invalid; Orc Cultists must be disabled");
    for (uint8 race : {12, 13})
    {
        auto* entry = sChrRacesStore.LookupEntry(race);
        if (!entry || entry->HasFlag(CHRRACES_FLAGS_NOT_PLAYABLE) ||
            !(sRaceMgr->GetPlayableRaceMask() & (1U << (race - 1))) || std::string_view(entry->name[0]) != "Dracthyr")
            throw std::runtime_error("ADVAI Dracthyr DBCs are missing or incompatible; install the backport bundle");
        for (uint8 cls : dracthyrClasses)
        {
            if (!sObjectMgr->GetPlayerInfo(race, cls))
                throw std::runtime_error(
                    "ADVAI Dracthyr creation data is incomplete; apply the optional world migration");
            if (!GetCharStartOutfitEntry(race, cls, GENDER_MALE) ||
                !GetCharStartOutfitEntry(race, cls, GENDER_FEMALE))
                throw std::runtime_error("ADVAI Dracthyr class outfit DBC is incomplete");
            if (!GetSkillRaceClassInfo(race == 12 ? 98 : 109, race, cls))
                throw std::runtime_error("ADVAI Dracthyr skill eligibility DBC is missing the faction language");
        }
        if (sObjectMgr->GetPlayerInfo(race, CLASS_DRUID))
            throw std::runtime_error("ADVAI race rules are invalid; Dracthyr Druids must be disabled");
    }
    for (uint32 spell : {GLIDE, TAIL_SWIPE, WING_BUFFET, VISAGE_MALE, VISAGE_FEMALE})
        if (!sSpellMgr->GetSpellInfo(spell))
            throw std::runtime_error("ADVAI Dracthyr racial spell catalog is incomplete");
    for (uint32 creature : {50004, 50005})
        if (!sObjectMgr->GetCreatureTemplate(creature))
            throw std::runtime_error("ADVAI Dracthyr Visage templates are missing");
    for (uint32 display : {10016, 10017})
        if (!sCreatureDisplayInfoStore.LookupEntry(display) || !sObjectMgr->GetCreatureModelInfo(display))
            throw std::runtime_error("ADVAI Dracthyr Visage display/model metadata is incomplete");
}

void Seed(Player* bot)
{
    if (!Manager::Instance().Applies(bot))
        return;
    if (Manager::Instance().Initialized(bot->GetGUID().GetCounter()))
        throw std::runtime_error("ADVAI refused repeated racial initialization");
    if (sPlayerbotAIConfig.AdvaiDracthyr && IsDracthyr(bot->getRace()))
    {
        for (uint32 spell :
             {GLIDE, TAIL_SWIPE, WING_BUFFET, bot->getGender() == GENDER_FEMALE ? VISAGE_FEMALE : VISAGE_MALE})
            bot->learnSpell(spell, false);
        bot->learnSpell(bot->GetTeamId() == TEAM_ALLIANCE ? 668 : 669, false);
    }
    auto profile = Manager::Instance().Profile(bot->GetGUID().GetCounter());
    if (Ascension::Enabled())
    {
        Ascension::SeedClass(bot);
        return;
    }
    for (auto const& ability : Abilities())
        if (ability.Profile == profile && bot->GetLevel() >= ability.Level)
            bot->learnSpell(ability.Spell, false);
}
}  // namespace Advai::Backport

std::vector<NextAction> AdvaiBackportStrategy::getDefaultActions()
{
    return {NextAction("advai racial", ACTION_EMERGENCY + 1.0f), NextAction("advai class", ACTION_NORMAL + 6.0f)};
}

uint32 AdvaiBackportStrategy::GetType() const
{
    uint32 spec = Advai::Ascension::Specialization(botAI->GetBot());
    if (!spec)
        return STRATEGY_TYPE_GENERIC;
    if (spec == 40)
        return STRATEGY_TYPE_HEAL | STRATEGY_TYPE_RANGED;
    if (spec == 96)
        return STRATEGY_TYPE_TANK | STRATEGY_TYPE_MELEE;
    return STRATEGY_TYPE_DPS | (spec == 42 ? STRATEGY_TYPE_MELEE : STRATEGY_TYPE_RANGED);
}

bool AdvaiRacialAction::isUseful()
{
    _spell = 0;
    auto& manager = Advai::Manager::Instance();
    if (!sPlayerbotAIConfig.AdvaiDracthyr || !manager.Applies(bot) || !Advai::Backport::IsDracthyr(bot->getRace()) ||
        !botAI->AllowActivity() || !bot->IsAlive() || bot->IsNonMeleeSpellCast(false) ||
        (_nextAttempt && static_cast<int32>(getMSTime() - _nextAttempt) < 0) || bot->IsInFlight() || bot->GetVehicle())
        return false;
    if (bot->IsFalling() && !bot->HasAura(Advai::Backport::GLIDE))
        _spell = Advai::Backport::GLIDE;
    else if (bot->IsInCombat() && bot->GetHealthPct() < 45 && !bot->GetGroup() && !bot->GetMap()->IsDungeon())
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (target && target->IsInWorld() && bot->IsWithinDistInMap(target, 8.0f))
            _spell = !bot->HasSpellCooldown(Advai::Backport::TAIL_SWIPE) ? Advai::Backport::TAIL_SWIPE
                                                                         : Advai::Backport::WING_BUFFET;
    }
    else if (!bot->IsInCombat() && bot->IsSitState() && !bot->isMoving() &&
             !bot->HasAura(Advai::Backport::VISAGE_MALE) && !bot->HasAura(Advai::Backport::VISAGE_FEMALE))
        _spell = bot->getGender() == GENDER_FEMALE ? Advai::Backport::VISAGE_FEMALE : Advai::Backport::VISAGE_MALE;
    return _spell && bot->HasSpell(_spell) && !bot->HasSpellCooldown(_spell);
}

bool AdvaiRacialAction::Execute(Event /*event*/)
{
    _nextAttempt = getMSTime() + 2000;
    if (!_spell)
        return false;
    if (_spell == Advai::Backport::WING_BUFFET)
        if (Unit* target = AI_VALUE(Unit*, "current target"))
            bot->SetFacingToObject(target);
    return botAI->CanCastSpell(_spell, bot) && botAI->CastSpell(_spell, bot);
}

bool AdvaiClassAction::isUseful()
{
    auto& manager = Advai::Manager::Instance();
    if (!manager.Applies(bot) || !botAI->AllowActivity() || !bot->IsAlive() || bot->IsInFlight() ||
        bot->IsNonMeleeSpellCast(false) || (_nextAttempt && static_cast<int32>(getMSTime() - _nextAttempt) < 0))
        return false;
    return manager.Profile(bot->GetGUID().GetCounter()) != Advai::ClassProfile::Classic;
}

bool AdvaiClassAction::Execute(Event /*event*/)
{
    using namespace Advai;
    using namespace Advai::Backport;
    auto& manager = Manager::Instance();
    auto id = bot->GetGUID().GetCounter();
    auto profile = manager.Profile(id);
    auto persona = manager.Persona(id);
    _nextAttempt = getMSTime() + 1000 + persona.ReactionMilliseconds();
    if (Ascension::Enabled())
    {
        if (!bot->IsInCombat())
        {
            if (_classLevel != bot->GetLevel())
                _classAdvanceComplete = false;
            if (!_classAdvanceComplete)
            {
                // AdvanceClass validates persisted progression and throws on inconsistent data; an
                // escaped exception here terminates the worldserver from the bot update tick.
                try
                {
                    _classAdvanceComplete = Ascension::AdvanceClass(bot) < 8;
                }
                catch (std::exception const& error)
                {
                    LOG_ERROR("playerbots", "ADVAI Ascension advancement disabled for {} (GUID {}): {}",
                              bot->GetName(), id, error.what());
                    _classAdvanceComplete = true;
                }
                _classLevel = bot->GetLevel();
            }
        }
        return Ascension::Act(botAI);
    }
    auto cast = [&](uint32 spell, Unit* target)
    {
        return spell && target && bot->HasSpell(spell) && !bot->HasSpellCooldown(spell) &&
               botAI->CanCastSpell(spell, target) && botAI->CastSpell(spell, target);
    };
    if (profile == ClassProfile::Necromancer)
    {
        if (Pet* pet = bot->GetPet())
        {
            uint32 group = bot->GetGroup() ? bot->GetGroup()->GetGUID().GetCounter() : 0;
            if (_configuredPet != pet->GetGUID().GetRawValue() || _petGroup != group)
            {
                for (auto const& [spellId, known] : pet->m_spells)
                    if (auto* info = sSpellMgr->GetSpellInfo(spellId);
                        info && pet->HasSpell(spellId) &&
                        (info->HasAura(SPELL_AURA_MOD_TAUNT) || info->HasEffect(SPELL_EFFECT_ATTACK_ME)))
                        pet->ToggleAutocast(info, !group);
                _configuredPet = pet->GetGUID().GetRawValue();
                _petGroup = group;
            }
        }
        else if (!bot->IsInCombat())
        {
            uint32 summon = RAISE_INITIATE;
            if (!bot->GetGroup() && persona.RiskTolerance < 50 && bot->HasSpell(RAISE_GOLEM) &&
                bot->GetItemCount(6265, false))
                summon = RAISE_GOLEM;
            else if (persona.Seed % 3 == 1 && bot->HasSpell(RAISE_MAGE))
                summon = RAISE_MAGE;
            else if (persona.Seed % 3 == 2 && bot->HasSpell(RAISE_ARCHER))
                summon = RAISE_ARCHER;
            return cast(summon, bot);
        }
    }
    if (!bot->IsInCombat())
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsInWorld() || !target->IsAlive() || !bot->IsValidAttackTarget(target) ||
        !bot->CanSeeOrDetect(target))
        return false;
    if (profile == ClassProfile::Cultist)
    {
        if (!bot->GetGuardianPet())
            return cast(bot->HasSpell(BIND_FACELESS) ? BIND_FACELESS : BIND_TENTACLE, target);
        return false;  // Existing Shadow Priest strategies execute the installed Cultist's core spell kit.
    }
    if (profile != ClassProfile::Necromancer)
        return false;
    if (bot->GetHealthPct() < 60 && !bot->HasAura(BONE_SHIELD) && cast(BONE_SHIELD, bot))
        return true;
    if (Pet* pet = bot->GetPet(); pet && pet->IsAlive() && pet->GetHealthPct() < 40 && cast(DEATH_COIL, pet))
        return true;
    if (bot->GetHealthPct() < 55)
        if (uint32 drain = AI_VALUE2(uint32, "spell id", "drain life"); cast(drain, target))
            return true;
    if (!bot->GetGroup() && AI_VALUE(GuidVector, "attackers").size() >= 2 && bot->HasSpell(CORPSE_EXPLOSION))
        if (auto raw = manager.RecentCorpse(id))
            if (Creature* corpse = ObjectAccessor::GetCreature(*bot, ObjectGuid(raw));
                corpse && !corpse->IsAlive() && corpse->loot.isLooted() && bot->IsWithinDistInMap(corpse, 20.0f) &&
                cast(CORPSE_EXPLOSION, corpse))
                return true;
    if (target->isMoving() && !target->HasAura(NECRO_FROSTBOLT, bot->GetGUID()) && cast(NECRO_FROSTBOLT, target))
        return true;
    if (!target->HasAura(55095, bot->GetGUID()) && cast(ICY_TOUCH, target))
        return true;
    if ((target->GetHealthPct() < 35 || persona.Competitiveness >= 50) && cast(DEATH_COIL, target))
        return true;
    return false;
}

namespace
{
using namespace std::chrono_literals;
class AdvaiBackportPlayerScript : public PlayerScript
{
public:
    AdvaiBackportPlayerScript()
        : PlayerScript("AdvaiBackportPlayerScript",
                       {PLAYERHOOK_CAN_LEARN_SPELL, PLAYERHOOK_ON_GET_TRAINER_SPELL_STATE, PLAYERHOOK_ON_CREATE})
    {
    }

    void OnPlayerCreate(Player* player) override
    {
        if (sPlayerbotAIConfig.AdvaiDracthyr && Advai::Backport::IsDracthyr(player->getRace()))
            player->learnSpell(
                player->getGender() == GENDER_FEMALE ? Advai::Backport::VISAGE_FEMALE : Advai::Backport::VISAGE_MALE,
                false);
    }

    static bool MayLearn(Player const* player, uint32 spell)
    {
        if (Advai::Ascension::Enabled())
        {
            if (spell >= Advai::Backport::NECRO_FROSTBOLT && spell <= 363422)
                return false;
            auto* info = sSpellMgr->GetSpellInfo(spell);
            if (!info)
                return false;
            auto& manager = Advai::Manager::Instance();
            auto profile = manager.Profile(player->GetGUID().GetCounter());
            if (info->SpellFamilyName == 29 || info->SpellFamilyName == 31)
            {
                auto expected =
                    info->SpellFamilyName == 29 ? Advai::ClassProfile::Necromancer : Advai::ClassProfile::Cultist;
                uint8 classId = info->SpellFamilyName == 29 ? CLASS_NECROMANCER : CLASS_CULTIST;
                return player->getClass() == classId && (!manager.Applies(player) || profile == expected);
            }
            if (manager.Applies(player) && profile != Advai::ClassProfile::Classic &&
                (info->SpellFamilyName == SPELLFAMILY_PRIEST || info->SpellFamilyName == SPELLFAMILY_WARLOCK))
                return false;
        }
        if (spell < Advai::Backport::NECRO_FROSTBOLT || spell > 363422)
            return true;
        for (auto const& ability : Advai::Backport::Abilities())
            if (ability.Spell == spell)
            {
                if (player->GetLevel() < ability.Level ||
                    player->getClass() !=
                        (ability.Profile == Advai::ClassProfile::Cultist ? CLASS_CULTIST : CLASS_NECROMANCER))
                    return false;
                if (Advai::Manager::Instance().Applies(player))
                    return Advai::Manager::Instance().Profile(player->GetGUID().GetCounter()) == ability.Profile;
                return true;
            }
        return false;
    }

    bool OnPlayerCanLearnSpell(Player* player, uint32 spell) override { return MayLearn(player, spell); }
    void OnPlayerGetTrainerSpellState(Player const* player, uint32 /*trainer*/, uint32 spell,
                                      Trainer::SpellState& state) override
    {
        if (!MayLearn(player, spell))
            state = Trainer::SpellState::Unavailable;
    }
};

class AdvaiVoidServantScript : public CreatureScript
{
public:
    AdvaiVoidServantScript() : CreatureScript("advai_void_servant") {}
    struct AI : public ScriptedAI
    {
        explicit AI(Creature* creature) : ScriptedAI(creature) {}
        ObjectGuid Owner;
        EventMap Events;
        void IsSummonedBy(WorldObject* summoner) override
        {
            Unit* owner = summoner ? summoner->ToUnit() : nullptr;
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }
            Owner = owner->GetGUID();
            if (me->GetEntry() == 95011)
            {
                me->SetLevel(owner->GetLevel());
                LOG_INFO("server.loading", "ADVAI Faceless summoned: owner level {}, servant level {}",
                         owner->GetLevel(), me->GetLevel());
            }
            me->SetReactState(REACT_DEFENSIVE);
            Events.ScheduleEvent(1, 500ms);
        }
        void UpdateAI(uint32 diff) override
        {
            Events.Update(diff);
            if (Events.ExecuteEvent() == 1)
            {
                Unit* owner = ObjectAccessor::GetUnit(*me, Owner);
                if (!owner || !owner->IsAlive() || !owner->IsInWorld())
                {
                    me->DespawnOrUnsummon();
                    return;
                }
                if (!me->GetVictim() && owner->IsInCombat())
                    if (Unit* enemy = owner->GetVictim(); enemy && enemy->IsAlive() && me->IsValidAttackTarget(enemy) &&
                                                          me->CanSeeOrDetect(enemy) &&
                                                          me->IsWithinDistInMap(enemy, 40.0f))
                    {
                        if (me->GetEntry() == 95011)
                            AttackStartCaster(enemy, 20.0f);
                        else
                            AttackStart(enemy);
                    }
                if (me->GetVictim() && !me->IsNonMeleeSpellCast(false) && me->GetEntry() == 95011)
                    DoCastVictim(Advai::Backport::SHADOW_ARROW);
                if (!me->GetVictim() && !me->HasUnitState(UNIT_STATE_FOLLOW))
                    me->GetMotionMaster()->MoveFollow(owner, 3.0f, 1.0f);
                Events.ScheduleEvent(1, 2500ms);
            }
            if (UpdateVictim() && me->GetEntry() != 95011)
                DoMeleeAttackIfReady();
        }
    };
    CreatureAI* GetAI(Creature* creature) const override
    {
        if (creature->GetEntry() == 95011)
            LOG_INFO("server.loading", "ADVAI Faceless AI selected for entry {}", creature->GetEntry());
        return new AI(creature);
    }
};
}  // namespace

void AddAdvaiBackportScripts()
{
    AddAdvaiAscensionScripts();
    new AdvaiBackportPlayerScript();
    new AdvaiVoidServantScript();
}
