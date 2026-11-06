/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_STATE_CODEC_H
#define PLAYERBOTS_ADVAI_STATE_CODEC_H

#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "AdvaiMgr.h"

namespace Advai::StateCodec
{
constexpr std::uint32_t STATE_VERSION = 10;

inline std::string Serialize(BotState const& s)
{
    std::ostringstream out;
    auto const& life = s.Life;
    auto const& goal = s.LifeGoal;
    out << STATE_VERSION << ' ' << life.Initialized << ' ' << life.WasPlayerGuild << ' ' << life.Joined << ' '
        << life.BreakDue << ' ' << life.ReturnAt << ' ' << life.SessionDue << ' ' << life.LoginAfter << ' '
        << life.GuildSatisfaction << ' ' << static_cast<unsigned>(goal.Kind) << ' ' << static_cast<unsigned>(goal.State)
        << ' ' << goal.Started << ' ' << goal.CommittedUntil << ' ' << goal.RetryAfter << ' ' << goal.Target << ' '
        << static_cast<unsigned>(goal.Failures) << ' ' << std::quoted(goal.Reason) << ' ' << s.Relationships.size();
    for (auto const& [id, relationship] : s.Relationships)
        out << ' ' << id << ' ' << relationship.Affinity << ' ' << relationship.Encounters << ' '
            << relationship.LastEvent << ' ' << relationship.LastReward << ' ' << relationship.Revision;
    auto const& c = s.Conversation;
    out << ' ' << c.Partner << ' ' << static_cast<unsigned>(c.Topic) << ' ' << c.LastMessage << ' ' << c.NextSpeak
        << ' ' << static_cast<unsigned>(c.Messages) << ' ' << c.RecentVariants.size();
    for (auto variant : c.RecentVariants)
        out << ' ' << variant;
    for (auto retry : s.RetryAfter)
        out << ' ' << retry;
    for (auto failures : s.Failures)
        out << ' ' << static_cast<unsigned>(failures);
    out << ' ' << c.QuestFocus << ' ' << s.Delivery.has_value();
    if (s.Delivery)
        out << ' ' << s.Delivery->ItemGuid << ' ' << s.Delivery->Item << ' ' << s.Delivery->Quantity << ' '
            << s.Delivery->Ordered << ' ' << s.Delivery->Deadline;
    out << ' ' << s.PartyLeader << ' ' << static_cast<unsigned>(s.PartyActivity) << ' ' << s.PartyJoined << ' '
        << static_cast<unsigned>(s.PartyWipes);
    out << ' ' << s.PendingRelationshipEvents.size();
    for (auto const& event : s.PendingRelationshipEvents)
        out << ' ' << static_cast<unsigned>(event.Kind) << ' ' << event.Other << ' ' << event.At << ' '
            << event.Positive << ' ' << event.Entity;
    out << ' ' << s.InterruptedGoals.size();
    for (auto const& interrupted : s.InterruptedGoals)
        out << ' ' << static_cast<unsigned>(interrupted.Kind) << ' ' << static_cast<unsigned>(interrupted.State)
            << ' ' << interrupted.Started << ' ' << interrupted.CommittedUntil << ' ' << interrupted.RetryAfter
            << ' ' << interrupted.Target << ' ' << static_cast<unsigned>(interrupted.Failures)
            << ' ' << std::quoted(interrupted.Reason);
    out << ' ' << s.CraftReservations.size();
    for (auto item : s.CraftReservations)
        out << ' ' << item;
    out << ' ' << life.GuildIdentityKnown << ' ' << life.GuildIdentity;
    out << ' ' << s.InitialSetupPending << ' ' << static_cast<unsigned>(s.InitialTargetLevel) << ' '
        << static_cast<unsigned>(s.Profile);
    out << ' ' << c.ItemFocus << ' ' << c.SpellFocus << ' ' << c.ProfessionFocus;
    return out.str();
}

inline void Deserialize(std::string const& payload, BotState& s, Timestamp now)
{
    std::istringstream in(payload);
    std::uint32_t version = 0, kind = 0, outcome = 0, failures = 0, topic = 0, messages = 0;
    std::size_t count = 0;
    auto& life = s.Life;
    auto& goal = s.LifeGoal;
    in >> version >> life.Initialized >> life.WasPlayerGuild >> life.Joined >> life.BreakDue >> life.ReturnAt >>
        life.SessionDue >> life.LoginAfter >> life.GuildSatisfaction >> kind >> outcome >> goal.Started >>
        goal.CommittedUntil >> goal.RetryAfter >> goal.Target >> failures >> std::quoted(goal.Reason) >> count;
    if (!in || (version < 2 || version > STATE_VERSION) || kind >= static_cast<unsigned>(Goal::Count) ||
        outcome > static_cast<unsigned>(Outcome::Cancelled) || failures > 8 || count > 100000 ||
        (version >= 5 && count > MAX_RESIDENT_CONTACTS))
        throw std::runtime_error("ADVAI invalid persistent state header");
    goal.Kind = static_cast<Goal>(kind);
    s.DepartureAfter = now + s.Persona.Seed % 300;
    goal.State = static_cast<Outcome>(outcome);
    goal.Failures = static_cast<std::uint8_t>(failures);
    s.LegacyRelationships = version < 5 && count != 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        BotId id = 0;
        Relationship relationship;
        in >> id >> relationship.Affinity >> relationship.Encounters >> relationship.LastEvent >>
            relationship.LastReward;
        if (version >= 5)
            in >> relationship.Revision;
        if (!in || !id || relationship.Affinity < -100 || relationship.Affinity > 100)
            throw std::runtime_error("ADVAI invalid persistent relationship");
        if (!s.Relationships.emplace(id, relationship).second)
            throw std::runtime_error("ADVAI duplicate persistent relationship");
    }
    auto& c = s.Conversation;
    in >> c.Partner >> topic >> c.LastMessage >> c.NextSpeak >> messages >> count;
    if (!in || topic >= static_cast<unsigned>(Intent::Count) || count > MAX_RECENT_LINES ||
        messages > MAX_CONVERSATION_MESSAGES)
        throw std::runtime_error("ADVAI invalid persistent conversation");
    c.Topic = static_cast<Intent>(topic);
    c.Messages = static_cast<std::uint8_t>(messages);
    for (std::size_t i = 0; i < count; ++i)
    {
        std::uint32_t variant = 0;
        in >> variant;
        c.RecentVariants.push_back(variant);
    }
    for (auto& retry : s.RetryAfter)
        in >> retry;
    for (auto& failuresForGoal : s.Failures)
    {
        unsigned value = 0;
        in >> value;
        if (value > 8)
            throw std::runtime_error("ADVAI invalid persistent goal failure count");
        failuresForGoal = static_cast<std::uint8_t>(value);
    }
    if (!in)
        throw std::runtime_error("ADVAI invalid goal retry state");
    if (version >= 3)
    {
        bool delivery = false;
        in >> c.QuestFocus >> delivery;
        if (delivery)
        {
            AuctionDelivery pending;
            in >> pending.ItemGuid >> pending.Item >> pending.Quantity >> pending.Ordered >> pending.Deadline;
            if (!in || !pending.ItemGuid || !pending.Item || !pending.Quantity || pending.Deadline <= pending.Ordered)
                throw std::runtime_error("ADVAI invalid persistent auction delivery");
            s.Delivery = pending;
        }
        if (!in)
            throw std::runtime_error("ADVAI invalid persistent delivery trailer");
    }
    if (version >= 4)
    {
        unsigned activity = 0, wipes = 0;
        in >> s.PartyLeader >> activity >> s.PartyJoined >> wipes;
        if (!in || activity >= static_cast<unsigned>(Goal::Count) || wipes > 20 ||
            (s.PartyLeader && activity == static_cast<unsigned>(Goal::None)))
            throw std::runtime_error("ADVAI invalid persistent party commitment");
        s.PartyActivity = static_cast<Goal>(activity);
        s.PartyWipes = static_cast<std::uint8_t>(wipes);
    }
    if (version >= 5)
    {
        std::size_t pending = 0;
        in >> pending;
        if (!in || pending > (version >= 7 ? MAX_PENDING_RELATIONSHIP_EVENTS : 64))
            throw std::runtime_error("ADVAI invalid pending relationship count");
        for (std::size_t index = 0; index < pending; ++index)
        {
            BotSocialEvent event;
            unsigned eventKind = 0;
            in >> eventKind >> event.Other >> event.At >> event.Positive >> event.Entity;
            if (!in || eventKind >= SOCIAL_EVENT_KINDS || !event.Other || !event.At)
                throw std::runtime_error("ADVAI invalid pending relationship observation");
            event.Kind = static_cast<SocialEventKind>(eventKind);
            s.PendingRelationshipEvents.push_back(event);
        }
    }
    s.InterruptedGoals.clear();
    s.CraftReservations.clear();
    if (version >= 9)
    {
        std::size_t interruptedCount = 0;
        in >> interruptedCount;
        if (!in || interruptedCount > MAX_INTERRUPTED_GOALS)
            throw std::runtime_error("ADVAI invalid interrupted goal count");
        std::unordered_set<Goal> kinds;
        s.InterruptedGoals.clear();
        for (std::size_t index = 0; index < interruptedCount; ++index)
        {
            BotLifeGoal interrupted;
            unsigned interruptedKind = 0, interruptedState = 0, interruptedFailures = 0;
            in >> interruptedKind >> interruptedState >> interrupted.Started >> interrupted.CommittedUntil >>
                interrupted.RetryAfter >> interrupted.Target >> interruptedFailures >> std::quoted(interrupted.Reason);
            if (!in || !interruptedKind || interruptedKind >= static_cast<unsigned>(Goal::Count) ||
                interruptedState > static_cast<unsigned>(Outcome::Blocked) ||
                interruptedState == static_cast<unsigned>(Outcome::Succeeded) || interruptedFailures > 8 ||
                interrupted.Reason.size() > 1024 || !kinds.insert(static_cast<Goal>(interruptedKind)).second)
                throw std::runtime_error("ADVAI invalid interrupted goal");
            interrupted.Kind = static_cast<Goal>(interruptedKind);
            interrupted.State = static_cast<Outcome>(interruptedState);
            interrupted.Failures = static_cast<std::uint8_t>(interruptedFailures);
            s.InterruptedGoals.push_back(std::move(interrupted));
        }
        std::size_t reservedCount = 0;
        in >> reservedCount;
        if (!in || reservedCount > 128)
            throw std::runtime_error("ADVAI invalid persistent craft reservation count");
        std::unordered_set<std::uint32_t> reserved;
        for (std::size_t index = 0; index < reservedCount; ++index)
        {
            std::uint32_t item = 0;
            in >> item;
            if (!in || !item || !reserved.insert(item).second)
                throw std::runtime_error("ADVAI invalid persistent craft reservation");
            s.CraftReservations.push_back(item);
        }
    }
    if (version >= 10)
    {
        in >> life.GuildIdentityKnown >> life.GuildIdentity;
        if (!in || (!life.GuildIdentityKnown && life.GuildIdentity))
            throw std::runtime_error("ADVAI invalid persistent guild identity");
    }
    else
    {
        life.GuildIdentityKnown = false;
        life.GuildIdentity = 0;
    }
    if (version >= 6)
    {
        unsigned initialLevel = 0, profile = 0;
        in >> s.InitialSetupPending >> initialLevel >> profile;
        if (!in || initialLevel > 255 || (life.Initialized && s.InitialSetupPending) ||
            (s.InitialSetupPending && !initialLevel) || profile >= static_cast<unsigned>(ClassProfile::Count))
            throw std::runtime_error("ADVAI inconsistent one-time initialization phase");
        s.InitialTargetLevel = static_cast<std::uint8_t>(initialLevel);
        s.Profile = static_cast<ClassProfile>(profile);
        s.InitialOriginDurable = s.InitialSetupPending;
    }
    if (version >= 7)
        in >> c.ItemFocus >> c.SpellFocus;
    if (version >= 8)
        in >> c.ProfessionFocus;
    if (!in)
        throw std::runtime_error("ADVAI invalid persistent entity context");
    in >> std::ws;
    if (!in.eof() || in.bad())
        throw std::runtime_error("ADVAI invalid persistent state trailer");
}

}  // namespace Advai::StateCodec

#endif
