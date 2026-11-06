/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_BACKPORT_H
#define PLAYERBOTS_ADVAI_BACKPORT_H

#include "Action.h"
#include "AdvaiModel.h"
#include "Strategy.h"

class Player;

namespace Advai::Backport
{
constexpr uint32 GLIDE = 363404;
constexpr uint32 TAIL_SWIPE = 363405;
constexpr uint32 WING_BUFFET = 363406;
constexpr uint32 VISAGE_MALE = 363402;
constexpr uint32 VISAGE_FEMALE = 363403;
constexpr uint32 NECRO_FROSTBOLT = 363410;
constexpr uint32 RAISE_INITIATE = 363411;
constexpr uint32 BONE_SHIELD = 363412;
constexpr uint32 CORPSE_EXPLOSION = 363413;
constexpr uint32 DEATH_COIL = 363414;
constexpr uint32 ICY_TOUCH = 363415;
constexpr uint32 RAISE_MAGE = 363416;
constexpr uint32 RAISE_ARCHER = 363417;
constexpr uint32 RAISE_GOLEM = 363418;
constexpr uint32 SHADOW_ARROW = 363419;
constexpr uint32 BIND_TENTACLE = 363420;
constexpr uint32 BIND_FACELESS = 363421;

struct ClassAbility
{
    uint32 Spell;
    uint8 Level;
    ClassProfile Profile;
};

bool IsDracthyr(uint8 race);
void Validate();
void Seed(Player* bot);
ClassProfile Assign(uint8 classId);
char const* Name(ClassProfile profile);
bool AllowsSpell(ClassProfile profile, uint32 spell);
std::vector<ClassAbility> const& Abilities();
}  // namespace Advai::Backport

class AdvaiBackportStrategy : public Strategy
{
public:
    explicit AdvaiBackportStrategy(PlayerbotAI* ai) : Strategy(ai) {}
    std::string const getName() override { return "advai backport"; }
    std::vector<NextAction> getDefaultActions() override;
    uint32 GetType() const override;
};

class AdvaiRacialAction : public Action
{
public:
    explicit AdvaiRacialAction(PlayerbotAI* ai) : Action(ai, "advai racial") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint32 _nextAttempt = 0;
    uint32 _spell = 0;
};

class AdvaiClassAction : public Action
{
public:
    explicit AdvaiClassAction(PlayerbotAI* ai) : Action(ai, "advai class") {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    uint32 _nextAttempt = 0;
    uint64 _configuredPet = 0;
    uint32 _petGroup = 0;
    uint8 _classLevel = 0;
    bool _classAdvanceComplete = false;
};

#endif
