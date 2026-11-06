/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_COGNITION_H
#define PLAYERBOTS_ADVAI_COGNITION_H

#include "AdvaiModel.h"

namespace Advai
{
constexpr std::size_t MAX_EXPERIENCES = 32;
constexpr std::size_t SOCIAL_EVENT_KINDS = 9;

struct BotMood
{
    std::int16_t Valence = 0;
    std::uint8_t Stress = 0;
    Timestamp Updated = 0;

    void Settle(Timestamp now);
    void Observe(BotSocialEvent const& event, BotPersona const& persona);
    std::uint32_t SocialDelay() const;
};

enum class AmbitionKind : std::uint8_t
{
    None,
    ReachLevel,
    DevelopProfession,
    BuildSavings,
    BuildFriendships,
    ImproveEquipment,
    Count
};

struct BotAmbition
{
    AmbitionKind Kind = AmbitionKind::None;
    std::uint32_t Baseline = 0;
    std::uint32_t Target = 0;
    std::uint32_t Progress = 0;
    std::uint32_t Completed = 0;
    Timestamp Started = 0;
    Timestamp CompletedAt = 0;

    void Update(BotActivitySnapshot const& snapshot, BotPersona const& persona, std::uint32_t friends);
    Goal Activity(BotActivitySnapshot const& snapshot, BotPersona const& persona) const;
    static char const* Name(AmbitionKind kind);
};

struct DurableSocialMemory
{
    std::array<std::uint32_t, SOCIAL_EVENT_KINDS> Positive{};
    std::array<std::uint32_t, SOCIAL_EVENT_KINDS> Negative{};
    std::deque<BotSocialEvent> Recent;

    bool Observe(BotSocialEvent const& event);
};

struct BotCognition
{
    BotMood Mood;
    BotAmbition Ambition;
    DurableSocialMemory Memory;

    bool Observe(BotSocialEvent const& event, BotPersona const& persona);
    std::string Serialize() const;
    static BotCognition Deserialize(std::string const& payload);
};
}  // namespace Advai

#endif
