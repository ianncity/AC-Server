/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_NECROMANCERAIOBJECTCONTEXT_H
#define PLAYERBOTS_NECROMANCERAIOBJECTCONTEXT_H

#include "AiObjectContext.h"

class PlayerbotAI;

class NecromancerAiObjectContext : public AiObjectContext
{
public:
    NecromancerAiObjectContext(PlayerbotAI* botAI);

    static void BuildSharedContexts();
    static void BuildSharedStrategyContexts(SharedNamedObjectContextList<Strategy>& strategyContexts);
    static void BuildSharedActionContexts(SharedNamedObjectContextList<Action>& actionContexts);
    static void BuildSharedTriggerContexts(SharedNamedObjectContextList<Trigger>& triggerContexts);
    static void BuildSharedValueContexts(SharedNamedObjectContextList<UntypedValue>& valueContexts);

    static SharedNamedObjectContextList<Strategy> sharedStrategyContexts;
    static SharedNamedObjectContextList<Action> sharedActionContexts;
    static SharedNamedObjectContextList<Trigger> sharedTriggerContexts;
    static SharedNamedObjectContextList<UntypedValue> sharedValueContexts;
};

#endif
