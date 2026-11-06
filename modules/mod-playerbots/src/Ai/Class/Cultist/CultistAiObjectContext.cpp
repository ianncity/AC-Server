/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "CultistAiObjectContext.h"
#include "CultistActions.h"
#include "CultistTriggers.h"
#include "DpsCultistStrategy.h"
#include "GenericCultistNonCombatStrategy.h"
#include "GenericCultistStrategy.h"
#include "HealerCultistStrategy.h"
#include "NamedObjectContext.h"
#include "Playerbots.h"
#include "PullStrategy.h"
#include "TankCultistStrategy.h"

class CultistStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    CultistStrategyFactoryInternal()
    {
        creators["nc"] = &CultistStrategyFactoryInternal::nc;
        creators["pull"] = &CultistStrategyFactoryInternal::pull;
        creators["aoe"] = &CultistStrategyFactoryInternal::aoe;
        creators["buff"] = &CultistStrategyFactoryInternal::buff;
    }

private:
    static Strategy* nc(PlayerbotAI* botAI) { return new GenericCultistNonCombatStrategy(botAI); }
    static Strategy* pull(PlayerbotAI* botAI) { return new PullStrategy(botAI, "horrorbolt"); }
    static Strategy* aoe(PlayerbotAI* botAI) { return new CultistAoeStrategy(botAI); }
    static Strategy* buff(PlayerbotAI* botAI) { return new CultistBuffStrategy(botAI); }
};

class CultistCombatStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    CultistCombatStrategyFactoryInternal() : NamedObjectContext<Strategy>(false, true)
    {
        creators["dps"] = &CultistCombatStrategyFactoryInternal::dps;
        creators["tank"] = &CultistCombatStrategyFactoryInternal::tank;
        creators["heal"] = &CultistCombatStrategyFactoryInternal::heal;
        creators["corruption"] = &CultistCombatStrategyFactoryInternal::dps;
        creators["godblade"] = &CultistCombatStrategyFactoryInternal::dps;
        creators["bulwark"] = &CultistCombatStrategyFactoryInternal::tank;
        creators["influence"] = &CultistCombatStrategyFactoryInternal::heal;
    }

private:
    static Strategy* dps(PlayerbotAI* botAI) { return new DpsCultistStrategy(botAI); }
    static Strategy* tank(PlayerbotAI* botAI) { return new TankCultistStrategy(botAI); }
    static Strategy* heal(PlayerbotAI* botAI) { return new HealerCultistStrategy(botAI); }
};

class CultistTriggerFactoryInternal : public NamedObjectContext<Trigger>
{
public:
    CultistTriggerFactoryInternal()
    {
        creators["void shield"] = &CultistTriggerFactoryInternal::void_shield;
        creators["void blessing"] = &CultistTriggerFactoryInternal::void_blessing;
        creators["wrath of the black empire"] = &CultistTriggerFactoryInternal::wrath_of_the_black_empire;
        creators["eldritch tentacle"] = &CultistTriggerFactoryInternal::eldritch_tentacle;
    }

private:
    static Trigger* void_shield(PlayerbotAI* botAI) { return new VoidShieldTrigger(botAI); }
    static Trigger* void_blessing(PlayerbotAI* botAI) { return new VoidBlessingTrigger(botAI); }
    static Trigger* wrath_of_the_black_empire(PlayerbotAI* botAI) { return new WrathOfTheBlackEmpireTrigger(botAI); }
    static Trigger* eldritch_tentacle(PlayerbotAI* botAI) { return new EldritchTentacleTrigger(botAI); }
};

class CultistActionFactoryInternal : public NamedObjectContext<Action>
{
public:
    CultistActionFactoryInternal()
    {
        creators["blade of the empire"] = &CultistActionFactoryInternal::blade_of_the_empire;
        creators["entropic slam"] = &CultistActionFactoryInternal::entropic_slam;
        creators["eldritch blast"] = &CultistActionFactoryInternal::eldritch_blast;
        creators["horrorbolt"] = &CultistActionFactoryInternal::horrorbolt;
        creators["wrath of the black empire"] = &CultistActionFactoryInternal::wrath_of_the_black_empire;
        creators["abyssal orb"] = &CultistActionFactoryInternal::abyssal_orb;
        creators["gaze of c'thun"] = &CultistActionFactoryInternal::gaze_of_cthun;
        creators["void shield"] = &CultistActionFactoryInternal::void_shield;
        creators["void blessing"] = &CultistActionFactoryInternal::void_blessing;
        creators["vision of doom"] = &CultistActionFactoryInternal::vision_of_doom;
        creators["eldritch mending"] = &CultistActionFactoryInternal::eldritch_mending;
        creators["eldritch mending on party"] = &CultistActionFactoryInternal::eldritch_mending_on_party;
        creators["abyssal reconstruction"] = &CultistActionFactoryInternal::abyssal_reconstruction;
        creators["abyssal reconstruction on party"] = &CultistActionFactoryInternal::abyssal_reconstruction_on_party;
        creators["restore sanity"] = &CultistActionFactoryInternal::restore_sanity;
        creators["eldritch tentacle"] = &CultistActionFactoryInternal::eldritch_tentacle;
        creators["tentacle of c'thun"] = &CultistActionFactoryInternal::tentacle_of_cthun;
    }

private:
    static Action* blade_of_the_empire(PlayerbotAI* botAI) { return new CastBladeOfTheEmpireAction(botAI); }
    static Action* entropic_slam(PlayerbotAI* botAI) { return new CastEntropicSlamAction(botAI); }
    static Action* eldritch_blast(PlayerbotAI* botAI) { return new CastEldritchBlastAction(botAI); }
    static Action* horrorbolt(PlayerbotAI* botAI) { return new CastHorrorboltAction(botAI); }
    static Action* wrath_of_the_black_empire(PlayerbotAI* botAI) { return new CastWrathOfTheBlackEmpireAction(botAI); }
    static Action* abyssal_orb(PlayerbotAI* botAI) { return new CastAbyssalOrbAction(botAI); }
    static Action* gaze_of_cthun(PlayerbotAI* botAI) { return new CastGazeOfCThunAction(botAI); }
    static Action* void_shield(PlayerbotAI* botAI) { return new CastVoidShieldAction(botAI); }
    static Action* void_blessing(PlayerbotAI* botAI) { return new CastVoidBlessingAction(botAI); }
    static Action* vision_of_doom(PlayerbotAI* botAI) { return new CastVisionOfDoomAction(botAI); }
    static Action* eldritch_mending(PlayerbotAI* botAI) { return new CastEldritchMendingAction(botAI); }
    static Action* eldritch_mending_on_party(PlayerbotAI* botAI) { return new CastEldritchMendingOnPartyAction(botAI); }
    static Action* abyssal_reconstruction(PlayerbotAI* botAI) { return new CastAbyssalReconstructionAction(botAI); }
    static Action* abyssal_reconstruction_on_party(PlayerbotAI* botAI) { return new CastAbyssalReconstructionOnPartyAction(botAI); }
    static Action* restore_sanity(PlayerbotAI* botAI) { return new CastRestoreSanityAction(botAI); }
    static Action* eldritch_tentacle(PlayerbotAI* botAI) { return new CastEldritchTentacleAction(botAI); }
    static Action* tentacle_of_cthun(PlayerbotAI* botAI) { return new CastTentacleOfCThunAction(botAI); }
};

SharedNamedObjectContextList<Strategy> CultistAiObjectContext::sharedStrategyContexts;
SharedNamedObjectContextList<Action> CultistAiObjectContext::sharedActionContexts;
SharedNamedObjectContextList<Trigger> CultistAiObjectContext::sharedTriggerContexts;
SharedNamedObjectContextList<UntypedValue> CultistAiObjectContext::sharedValueContexts;

CultistAiObjectContext::CultistAiObjectContext(PlayerbotAI* botAI)
    : AiObjectContext(botAI, sharedStrategyContexts, sharedActionContexts, sharedTriggerContexts, sharedValueContexts)
{
}

void CultistAiObjectContext::BuildSharedContexts()
{
    BuildSharedStrategyContexts(sharedStrategyContexts);
    BuildSharedActionContexts(sharedActionContexts);
    BuildSharedTriggerContexts(sharedTriggerContexts);
    BuildSharedValueContexts(sharedValueContexts);
}

void CultistAiObjectContext::BuildSharedStrategyContexts(SharedNamedObjectContextList<Strategy>& strategyContexts)
{
    AiObjectContext::BuildSharedStrategyContexts(strategyContexts);
    strategyContexts.Add(new CultistStrategyFactoryInternal());
    strategyContexts.Add(new CultistCombatStrategyFactoryInternal());
}

void CultistAiObjectContext::BuildSharedActionContexts(SharedNamedObjectContextList<Action>& actionContexts)
{
    AiObjectContext::BuildSharedActionContexts(actionContexts);
    actionContexts.Add(new CultistActionFactoryInternal());
}

void CultistAiObjectContext::BuildSharedTriggerContexts(SharedNamedObjectContextList<Trigger>& triggerContexts)
{
    AiObjectContext::BuildSharedTriggerContexts(triggerContexts);
    triggerContexts.Add(new CultistTriggerFactoryInternal());
}

void CultistAiObjectContext::BuildSharedValueContexts(SharedNamedObjectContextList<UntypedValue>& valueContexts)
{
    AiObjectContext::BuildSharedValueContexts(valueContexts);
}
