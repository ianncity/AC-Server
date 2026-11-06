/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_CULTISTAIOBJECTCONTEXT_H
#define PLAYERBOTS_CULTISTAIOBJECTCONTEXT_H

#include "AiObjectContext.h"

class PlayerbotAI;

class CultistAiObjectContext : public AiObjectContext
{
public:
    CultistAiObjectContext(PlayerbotAI* botAI);

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
