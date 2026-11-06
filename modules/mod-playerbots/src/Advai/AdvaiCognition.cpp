/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiCognition.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace Advai
{
namespace
{
constexpr Timestamp MOOD_STEP = 60;
constexpr Timestamp AMBITION_PAUSE = 300;
constexpr std::uint32_t MAX_LEVEL = 80;
constexpr std::uint32_t MAX_PROFESSION = 450;
constexpr std::uint32_t MAX_COPPER = 2147483647;

std::uint32_t ObserveProgress(BotActivitySnapshot const& snapshot, AmbitionKind kind, std::uint32_t friends)
{
    switch (kind)
    {
        case AmbitionKind::ReachLevel:
            return snapshot.Level;
        case AmbitionKind::DevelopProfession:
            return snapshot.ProfessionSkill;
        case AmbitionKind::BuildSavings:
            return snapshot.Money;
        case AmbitionKind::BuildFriendships:
            return friends;
        case AmbitionKind::ImproveEquipment:
            return snapshot.AverageItemLevel;
        default:
            return 0;
    }
}
}  // namespace

void BotMood::Settle(Timestamp now)
{
    if (!Updated)
        Updated = now;
    if (now <= Updated || now - Updated < MOOD_STEP)
        return;
    auto steps = static_cast<int>(std::min<Timestamp>(100, (now - Updated) / MOOD_STEP));
    Valence = static_cast<std::int16_t>(Valence > 0 ? std::max(0, Valence - steps) : std::min(0, Valence + steps));
    Stress = static_cast<std::uint8_t>(std::max(0, static_cast<int>(Stress) - steps));
    Updated += ((now - Updated) / MOOD_STEP) * MOOD_STEP;
}

void BotMood::Observe(BotSocialEvent const& event, BotPersona const& persona)
{
    Settle(event.At);
    int magnitude = event.Kind == SocialEventKind::Conversation ? 1 : 4 + (100 - persona.Patience) / 20;
    Valence =
        static_cast<std::int16_t>(std::clamp<int>(Valence + (event.Positive ? magnitude : -magnitude), -100, 100));
    int pressure = event.Kind == SocialEventKind::Death || event.Kind == SocialEventKind::Wipe ? 12 : magnitude;
    Stress = static_cast<std::uint8_t>(std::clamp<int>(Stress + (event.Positive ? -magnitude : pressure), 0, 100));
}

std::uint32_t BotMood::SocialDelay() const
{
    return static_cast<std::uint32_t>(Stress) * 2 + (Valence < 0 ? static_cast<std::uint32_t>(-Valence) : 0);
}

void BotAmbition::Update(BotActivitySnapshot const& snapshot, BotPersona const& persona, std::uint32_t friends)
{
    if (Kind != AmbitionKind::None && !CompletedAt)
    {
        Progress = ObserveProgress(snapshot, Kind, friends);
        if (Progress >= Target)
        {
            CompletedAt = snapshot.Now;
            if (Completed < std::numeric_limits<std::uint32_t>::max())
                ++Completed;
        }
        return;
    }
    if (CompletedAt && snapshot.Now < CompletedAt + AMBITION_PAUSE)
        return;
    if ((persona.Preference == Goal::Dungeon || persona.Preference == Goal::Raid ||
         persona.Preference == Goal::Arena || persona.Preference == Goal::Battleground) &&
        snapshot.Level >= 15 && snapshot.AverageItemLevel < (snapshot.Level < 80 ? snapshot.Level : 232) &&
        (Completed % 2 == 0 || snapshot.Level == MAX_LEVEL))
        Kind = AmbitionKind::ImproveEquipment;
    else if ((persona.Preference == Goal::Gather || persona.Preference == Goal::Craft) && snapshot.ProfessionSkill &&
        snapshot.ProfessionSkill < MAX_PROFESSION)
        Kind = AmbitionKind::DevelopProfession;
    else if (persona.Preference == Goal::Trade && snapshot.Money < MAX_COPPER)
        Kind = AmbitionKind::BuildSavings;
    else if (persona.Preference == Goal::Socialize && friends < 32)
        Kind = AmbitionKind::BuildFriendships;
    else if (snapshot.Level < MAX_LEVEL)
        Kind = AmbitionKind::ReachLevel;
    else if (snapshot.ProfessionSkill && snapshot.ProfessionSkill < MAX_PROFESSION)
        Kind = AmbitionKind::DevelopProfession;
    else if (snapshot.Money < MAX_COPPER)
        Kind = AmbitionKind::BuildSavings;
    else
        Kind = AmbitionKind::None;
    Baseline = ObserveProgress(snapshot, Kind, friends);
    std::uint32_t increment = 0;
    switch (Kind)
    {
        case AmbitionKind::ReachLevel:
            increment = std::min<std::uint32_t>(5, MAX_LEVEL - Baseline);
            break;
        case AmbitionKind::DevelopProfession:
            increment = std::min<std::uint32_t>(25, MAX_PROFESSION - Baseline);
            break;
        case AmbitionKind::BuildSavings:
            increment = std::min<std::uint32_t>(MAX_COPPER - Baseline, 1000 + snapshot.Level * 1000);
            break;
        case AmbitionKind::BuildFriendships:
            increment = std::min<std::uint32_t>(3, 32 - Baseline);
            break;
        case AmbitionKind::ImproveEquipment:
            increment = std::min<std::uint32_t>(10, (snapshot.Level < 80 ? snapshot.Level : 232) - Baseline);
            break;
        default:
            break;
    }
    Target = Baseline + increment;
    Progress = Baseline;
    Started = snapshot.Now;
    CompletedAt = 0;
}

Goal BotAmbition::Activity(BotActivitySnapshot const& snapshot, BotPersona const& persona) const
{
    if (CompletedAt)
        return Goal::None;
    switch (Kind)
    {
        case AmbitionKind::ReachLevel:
            // Preserve the character's preferred endgame and group activities while leveling.
            return persona.Preference == Goal::Quest || persona.Preference == Goal::Socialize ? Goal::Quest
                                                                                              : Goal::None;
        case AmbitionKind::DevelopProfession:
            if (snapshot.ProfessionActivity == Goal::Gather && snapshot.CanGather)
                return Goal::Gather;
            return snapshot.ProfessionActivity == Goal::Craft && snapshot.CanCraft ? Goal::Craft : Goal::None;
        case AmbitionKind::BuildSavings:
            if (persona.Preference == Goal::Trade || persona.Preference == Goal::Quest)
                return snapshot.HasAuctionStock ? Goal::Auction : Goal::Quest;
            return Goal::None;
        case AmbitionKind::BuildFriendships:
            return (snapshot.Now / 300 + persona.Seed) % 4 == 0 ? Goal::Socialize : Goal::None;
        case AmbitionKind::ImproveEquipment:
            if (persona.Preference == Goal::Arena || persona.Preference == Goal::Battleground)
                return persona.Preference == Goal::Arena && snapshot.Level < 70 ? Goal::Battleground : persona.Preference;
            return snapshot.Level >= 80 && persona.Preference == Goal::Raid ? Goal::Raid : Goal::Dungeon;
        default:
            return Goal::None;
    }
}

char const* BotAmbition::Name(AmbitionKind kind)
{
    constexpr std::array names = {"none", "reach level", "develop profession", "build savings", "build friendships",
                                  "improve equipment"};
    return static_cast<std::size_t>(kind) < names.size() ? names[static_cast<std::size_t>(kind)] : "invalid";
}

bool DurableSocialMemory::Observe(BotSocialEvent const& event)
{
    auto index = static_cast<std::size_t>(event.Kind);
    if (index >= SOCIAL_EVENT_KINDS || !event.At)
        throw std::invalid_argument("ADVAI invalid social experience");
    for (auto const& previous : Recent)
        if (previous.Kind == event.Kind && previous.Other == event.Other && previous.Entity == event.Entity &&
            previous.Positive == event.Positive && event.At <= previous.At + 60)
            return false;
    auto& count = event.Positive ? Positive[index] : Negative[index];
    if (count < std::numeric_limits<std::uint32_t>::max())
        ++count;
    Recent.push_back(event);
    if (Recent.size() > MAX_EXPERIENCES)
        Recent.pop_front();
    return true;
}

bool BotCognition::Observe(BotSocialEvent const& event, BotPersona const& persona)
{
    if (!Memory.Observe(event))
        return false;
    if (!(event.Kind == SocialEventKind::Death && event.Other))
        Mood.Observe(event, persona);
    return true;
}

std::string BotCognition::Serialize() const
{
    std::ostringstream out;
    out << 2 << ' ' << Mood.Valence << ' ' << unsigned(Mood.Stress) << ' ' << Mood.Updated << ' '
        << unsigned(Ambition.Kind) << ' ' << Ambition.Baseline << ' ' << Ambition.Target << ' ' << Ambition.Progress
        << ' ' << Ambition.Completed << ' ' << Ambition.Started << ' ' << Ambition.CompletedAt;
    for (auto count : Memory.Positive)
        out << ' ' << count;
    for (auto count : Memory.Negative)
        out << ' ' << count;
    out << ' ' << Memory.Recent.size();
    for (auto const& event : Memory.Recent)
        out << ' ' << unsigned(event.Kind) << ' ' << event.Other << ' ' << event.At << ' ' << event.Positive << ' '
            << event.Entity;
    return out.str();
}

BotCognition BotCognition::Deserialize(std::string const& payload)
{
    BotCognition result;
    std::istringstream in(payload);
    unsigned version = 0, stress = 0, kind = 0;
    std::size_t count = 0;
    int valence = 0;
    in >> version >> valence >> stress >> result.Mood.Updated >> kind >> result.Ambition.Baseline >>
        result.Ambition.Target >> result.Ambition.Progress >> result.Ambition.Completed >> result.Ambition.Started >>
        result.Ambition.CompletedAt;
    if (!in || version != 2 || valence < -100 || valence > 100 || stress > 100 ||
        kind >= unsigned(AmbitionKind::Count) || result.Ambition.Target > MAX_COPPER ||
        (kind != unsigned(AmbitionKind::None) && result.Ambition.Target <= result.Ambition.Baseline))
        throw std::runtime_error("ADVAI invalid persistent cognition header");
    result.Mood.Valence = static_cast<std::int16_t>(valence);
    result.Mood.Stress = static_cast<std::uint8_t>(stress);
    result.Ambition.Kind = static_cast<AmbitionKind>(kind);
    for (auto& value : result.Memory.Positive)
        in >> value;
    for (auto& value : result.Memory.Negative)
        in >> value;
    in >> count;
    if (!in || count > MAX_EXPERIENCES)
        throw std::runtime_error("ADVAI invalid persistent experience count");
    for (std::size_t i = 0; i < count; ++i)
    {
        BotSocialEvent event;
        in >> kind >> event.Other >> event.At >> event.Positive >> event.Entity;
        if (!in || kind >= SOCIAL_EVENT_KINDS || !event.At)
            throw std::runtime_error("ADVAI invalid persistent experience");
        event.Kind = static_cast<SocialEventKind>(kind);
        result.Memory.Recent.push_back(event);
    }
    in >> std::ws;
    if (!in.eof() || in.bad())
        throw std::runtime_error("ADVAI invalid persistent cognition trailer");
    return result;
}
}  // namespace Advai
