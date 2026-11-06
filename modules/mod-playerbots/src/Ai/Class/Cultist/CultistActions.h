/*
 * This file is part of the mod-playerbots module for AzerothCore.
 */

#ifndef PLAYERBOTS_CULTISTACTIONS_H
#define PLAYERBOTS_CULTISTACTIONS_H

#include "Action.h"
#include "GenericSpellActions.h"

class PlayerbotAI;

// Melee Attacks
class CastBladeOfTheEmpireAction : public CastMeleeSpellAction
{
public:
    CastBladeOfTheEmpireAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "blade of the empire") {}
};

class CastEntropicSlamAction : public CastMeleeSpellAction
{
public:
    CastEntropicSlamAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "entropic slam") {}
};

// Ranged Spells
class CastEldritchBlastAction : public CastSpellAction
{
public:
    CastEldritchBlastAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "eldritch blast") {}
};

class CastHorrorboltAction : public CastSpellAction
{
public:
    CastHorrorboltAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "horrorbolt") {}
};

class CastWrathOfTheBlackEmpireAction : public CastDebuffSpellAction
{
public:
    CastWrathOfTheBlackEmpireAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "wrath of the black empire") {}
};

class CastAbyssalOrbAction : public CastSpellAction
{
public:
    CastAbyssalOrbAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "abyssal orb") {}
};

class CastGazeOfCThunAction : public CastSpellAction
{
public:
    CastGazeOfCThunAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "gaze of c'thun") {}
};

// Defensive & Buffs
class CastVoidShieldAction : public CastBuffSpellAction
{
public:
    CastVoidShieldAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "void shield") {}
};

class CastVoidBlessingAction : public CastBuffSpellAction
{
public:
    CastVoidBlessingAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "void blessing") {}
};

class CastVisionOfDoomAction : public CastBuffSpellAction
{
public:
    CastVisionOfDoomAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "vision of doom") {}
};

// Healing
class CastEldritchMendingAction : public CastAuraSpellAction
{
public:
    CastEldritchMendingAction(PlayerbotAI* botAI) : CastAuraSpellAction(botAI, "eldritch mending") {}
    std::string const GetTargetName() override { return "self target"; }
};

class CastEldritchMendingOnPartyAction : public HealPartyMemberAction
{
public:
    CastEldritchMendingOnPartyAction(PlayerbotAI* botAI) : HealPartyMemberAction(botAI, "eldritch mending") {}
};

class CastAbyssalReconstructionAction : public CastSpellAction
{
public:
    CastAbyssalReconstructionAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "abyssal reconstruction") {}
    std::string const GetTargetName() override { return "self target"; }
};

class CastAbyssalReconstructionOnPartyAction : public HealPartyMemberAction
{
public:
    CastAbyssalReconstructionOnPartyAction(PlayerbotAI* botAI) : HealPartyMemberAction(botAI, "abyssal reconstruction") {}
};

class CastRestoreSanityAction : public CastSpellAction
{
public:
    CastRestoreSanityAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "restore sanity") {}
};

// Summons
class CastEldritchTentacleAction : public CastBuffSpellAction
{
public:
    CastEldritchTentacleAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "eldritch tentacle") {}
};

class CastTentacleOfCThunAction : public CastBuffSpellAction
{
public:
    CastTentacleOfCThunAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "tentacle of c'thun") {}
};

#endif
