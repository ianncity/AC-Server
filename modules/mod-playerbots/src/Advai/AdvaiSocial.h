/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_SOCIAL_H
#define PLAYERBOTS_ADVAI_SOCIAL_H

#include <vector>

#include "AdvaiModel.h"
#include "PlayerbotOperation.h"

class Group;

namespace Advai
{
std::uint64_t RaidRosterSignature(Group* group);

class FlightOperation : public PlayerbotOperation
{
public:
    FlightOperation(ObjectGuid bot, ObjectGuid master, std::vector<uint32> nodes, Goal purpose = Goal::Travel)
        : _bot(bot), _master(master), _nodes(std::move(nodes)), _purpose(purpose)
    {
    }
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "take a discovered paid flight"; }

private:
    ObjectGuid _bot;
    ObjectGuid _master;
    std::vector<uint32> _nodes;
    Goal _purpose;
};

class TrainOperation : public PlayerbotOperation
{
public:
    TrainOperation(ObjectGuid bot, ObjectGuid trainer, uint32 spell = 0) : _bot(bot), _trainer(trainer), _spell(spell)
    {
    }
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI learn affordable trainer spells"; }

private:
    ObjectGuid _bot;
    ObjectGuid _trainer;
    uint32 _spell;
};

class PartyUpdateOperation : public PlayerbotOperation
{
public:
    explicit PartyUpdateOperation(ObjectGuid bot) : _bot(bot) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI coordinate party preparation and recovery"; }

private:
    ObjectGuid _bot;
};

class DungeonJoinOperation : public PlayerbotOperation
{
public:
    DungeonJoinOperation(ObjectGuid bot, uint8 roles, std::vector<uint32> dungeons)
        : _bot(bot), _roles(roles), _dungeons(std::move(dungeons))
    {
    }
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI join eligible dungeon activity"; }

private:
    ObjectGuid _bot;
    uint8 _roles;
    std::vector<uint32> _dungeons;
};

class AcceptInviteOperation : public PlayerbotOperation
{
public:
    AcceptInviteOperation(ObjectGuid bot, ObjectGuid inviter) : _bot(bot), _inviter(inviter) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    uint32 GetPriority() const override { return 50; }
    std::string GetName() const override { return "ADVAI accept permitted group invitation"; }

private:
    ObjectGuid _bot;
    ObjectGuid _inviter;
};

class RaidAssemblyOperation : public PlayerbotOperation
{
public:
    RaidAssemblyOperation(ObjectGuid bot, std::uint32_t dungeon) : _bot(bot), _dungeon(dungeon) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI recruit eligible raid roles"; }

private:
    ObjectGuid _bot;
    std::uint32_t _dungeon;
};

class EnterRaidOperation : public PlayerbotOperation
{
public:
    EnterRaidOperation(ObjectGuid bot, std::uint32_t dungeon, std::uint32_t trigger)
        : _bot(bot), _dungeon(dungeon), _trigger(trigger) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI enter raid through its real portal"; }

private:
    ObjectGuid _bot;
    std::uint32_t _dungeon;
    std::uint32_t _trigger;
};

class PortalUseOperation : public PlayerbotOperation
{
public:
    PortalUseOperation(ObjectGuid bot, ObjectGuid portal, Goal purpose)
        : _bot(bot), _portal(portal), _purpose(purpose) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI use a real accessible portal"; }

private:
    ObjectGuid _bot;
    ObjectGuid _portal;
    Goal _purpose;
};

class AreaTriggerUseOperation : public PlayerbotOperation
{
public:
    AreaTriggerUseOperation(ObjectGuid bot, std::uint32_t trigger, Goal purpose)
        : _bot(bot), _trigger(trigger), _purpose(purpose) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI use a reached area trigger"; }

private:
    ObjectGuid _bot;
    std::uint32_t _trigger;
    Goal _purpose;
};

class InviteOperation : public PlayerbotOperation
{
public:
    InviteOperation(ObjectGuid bot, ObjectGuid peer) : _bot(bot), _peer(peer) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    uint32 GetPriority() const override { return 50; }
    std::string GetName() const override { return "ADVAI send group invitation"; }

private:
    ObjectGuid _bot;
    ObjectGuid _peer;
};

class GuildInviteOperation : public PlayerbotOperation
{
public:
    GuildInviteOperation(ObjectGuid bot, ObjectGuid peer) : _bot(bot), _peer(peer) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI send requested guild invitation"; }

private:
    ObjectGuid _bot;
    ObjectGuid _peer;
};

class ArenaCharterOperation : public PlayerbotOperation
{
public:
    ArenaCharterOperation(ObjectGuid bot, ObjectGuid vendor) : _bot(bot), _vendor(vendor) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI paid arena charter and signatures"; }

private:
    ObjectGuid _bot;
    ObjectGuid _vendor;
};

class ArenaAssemblyOperation : public PlayerbotOperation
{
public:
    ArenaAssemblyOperation(ObjectGuid bot, uint8 type) : _bot(bot), _type(type) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI invite available arena teammates"; }

private:
    ObjectGuid _bot;
    uint8 _type;
};
}  // namespace Advai

#endif
