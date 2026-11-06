/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_NECROMANCERACTIONS_H
#define PLAYERBOTS_NECROMANCERACTIONS_H

#include "Action.h"
#include "GenericSpellActions.h"

class PlayerbotAI;

// Spells
class CastLichfrostAction : public CastSpellAction
{
public:
    CastLichfrostAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "lichfrost") {}
};

class CastPlagueOfUndeathAction : public CastDebuffSpellAction
{
public:
    CastPlagueOfUndeathAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "plague of undeath") {}
};

class CastHarvestPlagueAction : public CastDebuffSpellAction
{
public:
    CastHarvestPlagueAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "harvest plague") {}
};

class CastBlightAction : public CastSpellAction
{
public:
    CastBlightAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "blight") {}
};

class CastCorpseExplosionAction : public CastSpellAction
{
public:
    CastCorpseExplosionAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "corpse explosion") {}
};

class CastChillOfTheTombAction : public CastSpellAction
{
public:
    CastChillOfTheTombAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "chill of the tomb") {}
};

class CastHeartchillAction : public CastSpellAction
{
public:
    CastHeartchillAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "heartchill") {}
};

class CastSiphonManaAction : public CastSpellAction
{
public:
    CastSiphonManaAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "siphon mana") {}
};

class CastTransferLifeAction : public CastSpellAction
{
public:
    CastTransferLifeAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "transfer life") {}
};

// Buffs & Stances
class CastUndeadAssaultAction : public CastBuffSpellAction
{
public:
    CastUndeadAssaultAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "undead: assault") {}
};

class CastUndeadProtectAction : public CastBuffSpellAction
{
public:
    CastUndeadProtectAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "undead: protect") {}
};

class CastFoulMandateAction : public CastBuffSpellAction
{
public:
    CastFoulMandateAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "foul mandate") {}
};

// Summons
class CastRaiseLesserSkeletalWarriorAction : public CastBuffSpellAction
{
public:
    CastRaiseLesserSkeletalWarriorAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "raise: lesser skeletal warrior") {}
};

class CastRaiseGreaterSkeletalWarriorAction : public CastBuffSpellAction
{
public:
    CastRaiseGreaterSkeletalWarriorAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "raise: greater skeletal warrior") {}
};

class CastAnimateRotlingAction : public CastBuffSpellAction
{
public:
    CastAnimateRotlingAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "animate: rotling") {}
};

class CastAnimateSkeletalSmithAction : public CastBuffSpellAction
{
public:
    CastAnimateSkeletalSmithAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "animate: skeletal smith") {}
};

class CastAnimatePlaguefatherAction : public CastBuffSpellAction
{
public:
    CastAnimatePlaguefatherAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "animate: plaguefather") {}
};

#endif
