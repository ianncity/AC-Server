/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_DIALOGUE_H
#define PLAYERBOTS_ADVAI_DIALOGUE_H

#include "AdvaiCognition.h"

namespace Advai
{
struct DialogueContext
{
    bool PublicAudience = false;
    bool KnownPartner = false;
    std::int16_t Affinity = 0;
    bool PreferStatement = false;
    bool RequireQuestion = false;
};

struct DeliveredSpeech
{
    std::uint64_t Thread = 0;
    std::string Text;
    Intent Topic = Intent::Unknown;
    std::string Source;
    Timestamp Expires = 0;
};

class DeliveredSpeechLedger
{
public:
    void Record(BotId sender, DeliveredSpeech speech);
    std::optional<DeliveredSpeech> Find(BotId sender, std::string const& text, Timestamp now) const;
    void Revoke(BotId sender, std::uint64_t thread, std::string const& text);
    void Prune(Timestamp now);

private:
    static constexpr std::size_t LINES_PER_SENDER = 8;
    std::unordered_map<BotId, std::deque<DeliveredSpeech>> _lines;
};

struct SpeechTraits
{
    std::uint32_t Id = 0;
    std::uint16_t Bytes = 0;
    bool Casual = false;
    bool Energetic = false;
    bool Familiar = false;
    bool Collective = false;
    bool Question = false;

    static SpeechTraits Analyze(std::string const& key, std::string const& english);
    std::uint32_t Weight(BotPersona const& persona, BotMood const& mood, DialogueContext const& context) const;
};

std::size_t SelectSpeech(std::vector<SpeechTraits> const& profiles, BotPersona const& persona, BotMood const& mood,
                         DialogueContext const& context, std::deque<std::uint32_t> const& recent,
                         std::uint32_t selector);
}  // namespace Advai

#endif
