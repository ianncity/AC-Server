/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#include "NecromancerAiObjectContext.h"
#include "DpsNecromancerStrategy.h"
#include "GenericNecromancerNonCombatStrategy.h"
#include "GenericNecromancerStrategy.h"
#include "NamedObjectContext.h"
#include "NecromancerActions.h"
#include "NecromancerTriggers.h"
#include "Playerbots.h"
#include "PullStrategy.h"
#include "SummonerNecromancerStrategy.h"
#include "TankNecromancerStrategy.h"

class NecromancerStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    NecromancerStrategyFactoryInternal()
    {
        creators["nc"] = &NecromancerStrategyFactoryInternal::nc;
        creators["pull"] = &NecromancerStrategyFactoryInternal::pull;
        creators["aoe"] = &NecromancerStrategyFactoryInternal::aoe;
        creators["buff"] = &NecromancerStrategyFactoryInternal::buff;
    }

private:
    static Strategy* nc(PlayerbotAI* botAI) { return new GenericNecromancerNonCombatStrategy(botAI); }
    static Strategy* pull(PlayerbotAI* botAI) { return new PullStrategy(botAI, "lichfrost"); }
    static Strategy* aoe(PlayerbotAI* botAI) { return new NecromancerAoeStrategy(botAI); }
    static Strategy* buff(PlayerbotAI* botAI) { return new NecromancerBuffStrategy(botAI); }
};

class NecromancerCombatStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    NecromancerCombatStrategyFactoryInternal() : NamedObjectContext<Strategy>(false, true)
    {
        creators["dps"] = &NecromancerCombatStrategyFactoryInternal::dps;
        creators["tank"] = &NecromancerCombatStrategyFactoryInternal::tank;
        creators["summon"] = &NecromancerCombatStrategyFactoryInternal::summon;
        creators["death"] = &NecromancerCombatStrategyFactoryInternal::dps;
        creators["rime"] = &NecromancerCombatStrategyFactoryInternal::dps;
        creators["animation"] = &NecromancerCombatStrategyFactoryInternal::summon;
    }

private:
    static Strategy* dps(PlayerbotAI* botAI) { return new DpsNecromancerStrategy(botAI); }
    static Strategy* tank(PlayerbotAI* botAI) { return new TankNecromancerStrategy(botAI); }
    static Strategy* summon(PlayerbotAI* botAI) { return new SummonerNecromancerStrategy(botAI); }
};

class NecromancerTriggerFactoryInternal : public NamedObjectContext<Trigger>
{
public:
    NecromancerTriggerFactoryInternal()
    {
        creators["plague of undeath"] = &NecromancerTriggerFactoryInternal::plague_of_undeath;
        creators["harvest plague"] = &NecromancerTriggerFactoryInternal::harvest_plague;
        creators["undead: protect"] = &NecromancerTriggerFactoryInternal::undead_protect;
        creators["undead: assault"] = &NecromancerTriggerFactoryInternal::undead_assault;
        creators["foul mandate"] = &NecromancerTriggerFactoryInternal::foul_mandate;
        creators["no pet"] = &NecromancerTriggerFactoryInternal::no_pet;
    }

private:
    static Trigger* plague_of_undeath(PlayerbotAI* botAI) { return new PlagueOfUndeathTrigger(botAI); }
    static Trigger* harvest_plague(PlayerbotAI* botAI) { return new HarvestPlagueTrigger(botAI); }
    static Trigger* undead_protect(PlayerbotAI* botAI) { return new UndeadProtectTrigger(botAI); }
    static Trigger* undead_assault(PlayerbotAI* botAI) { return new UndeadAssaultTrigger(botAI); }
    static Trigger* foul_mandate(PlayerbotAI* botAI) { return new FoulMandateTrigger(botAI); }
    static Trigger* no_pet(PlayerbotAI* botAI) { return new NecromancerNoPetTrigger(botAI); }
};

class NecromancerActionFactoryInternal : public NamedObjectContext<Action>
{
public:
    NecromancerActionFactoryInternal()
    {
        creators["lichfrost"] = &NecromancerActionFactoryInternal::lichfrost;
        creators["plague of undeath"] = &NecromancerActionFactoryInternal::plague_of_undeath;
        creators["harvest plague"] = &NecromancerActionFactoryInternal::harvest_plague;
        creators["blight"] = &NecromancerActionFactoryInternal::blight;
        creators["corpse explosion"] = &NecromancerActionFactoryInternal::corpse_explosion;
        creators["chill of the tomb"] = &NecromancerActionFactoryInternal::chill_of_the_tomb;
        creators["heartchill"] = &NecromancerActionFactoryInternal::heartchill;
        creators["siphon mana"] = &NecromancerActionFactoryInternal::siphon_mana;
        creators["transfer life"] = &NecromancerActionFactoryInternal::transfer_life;
        creators["undead: assault"] = &NecromancerActionFactoryInternal::undead_assault;
        creators["undead: protect"] = &NecromancerActionFactoryInternal::undead_protect;
        creators["foul mandate"] = &NecromancerActionFactoryInternal::foul_mandate;
        creators["raise: lesser skeletal warrior"] = &NecromancerActionFactoryInternal::raise_lesser_skeletal_warrior;
        creators["raise: greater skeletal warrior"] = &NecromancerActionFactoryInternal::raise_greater_skeletal_warrior;
        creators["animate: rotling"] = &NecromancerActionFactoryInternal::animate_rotling;
        creators["animate: skeletal smith"] = &NecromancerActionFactoryInternal::animate_skeletal_smith;
        creators["animate: plaguefather"] = &NecromancerActionFactoryInternal::animate_plaguefather;
    }

private:
    static Action* lichfrost(PlayerbotAI* botAI) { return new CastLichfrostAction(botAI); }
    static Action* plague_of_undeath(PlayerbotAI* botAI) { return new CastPlagueOfUndeathAction(botAI); }
    static Action* harvest_plague(PlayerbotAI* botAI) { return new CastHarvestPlagueAction(botAI); }
    static Action* blight(PlayerbotAI* botAI) { return new CastBlightAction(botAI); }
    static Action* corpse_explosion(PlayerbotAI* botAI) { return new CastCorpseExplosionAction(botAI); }
    static Action* chill_of_the_tomb(PlayerbotAI* botAI) { return new CastChillOfTheTombAction(botAI); }
    static Action* heartchill(PlayerbotAI* botAI) { return new CastHeartchillAction(botAI); }
    static Action* siphon_mana(PlayerbotAI* botAI) { return new CastSiphonManaAction(botAI); }
    static Action* transfer_life(PlayerbotAI* botAI) { return new CastTransferLifeAction(botAI); }
    static Action* undead_assault(PlayerbotAI* botAI) { return new CastUndeadAssaultAction(botAI); }
    static Action* undead_protect(PlayerbotAI* botAI) { return new CastUndeadProtectAction(botAI); }
    static Action* foul_mandate(PlayerbotAI* botAI) { return new CastFoulMandateAction(botAI); }
    static Action* raise_lesser_skeletal_warrior(PlayerbotAI* botAI) { return new CastRaiseLesserSkeletalWarriorAction(botAI); }
    static Action* raise_greater_skeletal_warrior(PlayerbotAI* botAI) { return new CastRaiseGreaterSkeletalWarriorAction(botAI); }
    static Action* animate_rotling(PlayerbotAI* botAI) { return new CastAnimateRotlingAction(botAI); }
    static Action* animate_skeletal_smith(PlayerbotAI* botAI) { return new CastAnimateSkeletalSmithAction(botAI); }
    static Action* animate_plaguefather(PlayerbotAI* botAI) { return new CastAnimatePlaguefatherAction(botAI); }
};

SharedNamedObjectContextList<Strategy> NecromancerAiObjectContext::sharedStrategyContexts;
SharedNamedObjectContextList<Action> NecromancerAiObjectContext::sharedActionContexts;
SharedNamedObjectContextList<Trigger> NecromancerAiObjectContext::sharedTriggerContexts;
SharedNamedObjectContextList<UntypedValue> NecromancerAiObjectContext::sharedValueContexts;

NecromancerAiObjectContext::NecromancerAiObjectContext(PlayerbotAI* botAI)
    : AiObjectContext(botAI, sharedStrategyContexts, sharedActionContexts, sharedTriggerContexts, sharedValueContexts)
{
}

void NecromancerAiObjectContext::BuildSharedContexts()
{
    BuildSharedStrategyContexts(sharedStrategyContexts);
    BuildSharedActionContexts(sharedActionContexts);
    BuildSharedTriggerContexts(sharedTriggerContexts);
    BuildSharedValueContexts(sharedValueContexts);
}

void NecromancerAiObjectContext::BuildSharedStrategyContexts(SharedNamedObjectContextList<Strategy>& strategyContexts)
{
    AiObjectContext::BuildSharedStrategyContexts(strategyContexts);
    strategyContexts.Add(new NecromancerStrategyFactoryInternal());
    strategyContexts.Add(new NecromancerCombatStrategyFactoryInternal());
}

void NecromancerAiObjectContext::BuildSharedActionContexts(SharedNamedObjectContextList<Action>& actionContexts)
{
    AiObjectContext::BuildSharedActionContexts(actionContexts);
    actionContexts.Add(new NecromancerActionFactoryInternal());
}

void NecromancerAiObjectContext::BuildSharedTriggerContexts(SharedNamedObjectContextList<Trigger>& triggerContexts)
{
    AiObjectContext::BuildSharedTriggerContexts(triggerContexts);
    triggerContexts.Add(new NecromancerTriggerFactoryInternal());
}

void NecromancerAiObjectContext::BuildSharedValueContexts(SharedNamedObjectContextList<UntypedValue>& valueContexts)
{
    AiObjectContext::BuildSharedValueContexts(valueContexts);
}
