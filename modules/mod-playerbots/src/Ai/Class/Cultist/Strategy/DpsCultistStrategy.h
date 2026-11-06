/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_DPSCULTISTSTRATEGY_H
#define PLAYERBOTS_DPSCULTISTSTRATEGY_H

#include "GenericCultistStrategy.h"

class PlayerbotAI;

class DpsCultistStrategy : public GenericCultistStrategy
{
public:
    DpsCultistStrategy(PlayerbotAI* botAI);

    std::string const getName() override { return "dps"; }
    void InitTriggers(std::vector<TriggerNode*>& triggers) override;
    std::vector<NextAction> getDefaultActions() override;
};

#endif
