/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiDialogue.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace Advai
{
void DeliveredSpeechLedger::Record(BotId sender, DeliveredSpeech speech)
{
    if (!sender || speech.Text.empty() || speech.Source.empty() || !speech.Expires)
        throw std::invalid_argument("ADVAI delivered speech requires sender, actual text, source and expiry");
    auto& lines = _lines[sender];
    lines.push_back(std::move(speech));
    if (lines.size() > LINES_PER_SENDER)
        lines.pop_front();
}

std::optional<DeliveredSpeech> DeliveredSpeechLedger::Find(BotId sender, std::string const& text, Timestamp now) const
{
    auto found = _lines.find(sender);
    if (found == _lines.end())
        return std::nullopt;
    for (auto line = found->second.rbegin(); line != found->second.rend(); ++line)
        if (line->Expires > now && line->Text == text)
            return *line;
    return std::nullopt;
}

void DeliveredSpeechLedger::Revoke(BotId sender, std::uint64_t thread, std::string const& text)
{
    auto found = _lines.find(sender);
    if (found == _lines.end())
        return;
    std::erase_if(found->second, [&](auto const& line) { return line.Thread == thread && line.Text == text; });
    if (found->second.empty())
        _lines.erase(found);
}

void DeliveredSpeechLedger::Prune(Timestamp now)
{
    for (auto entry = _lines.begin(); entry != _lines.end();)
    {
        std::erase_if(entry->second, [&](auto const& line) { return line.Expires <= now; });
        if (entry->second.empty())
            entry = _lines.erase(entry);
        else
            ++entry;
    }
}

SpeechTraits SpeechTraits::Analyze(std::string const& key, std::string const& english)
{
    SpeechTraits result;
    std::uint32_t hash = 2166136261U;
    for (unsigned char c : key)
        hash = (hash ^ c) * 16777619U;
    hash = (hash ^ 0) * 16777619U;
    for (unsigned char c : english)
        hash = (hash ^ c) * 16777619U;
    result.Id = hash;
    result.Bytes = static_cast<std::uint16_t>(std::min<std::size_t>(MAX_CHAT_BYTES, english.size()));
    std::string normalized = " ";
    for (unsigned char c : english)
        normalized += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : ' ';
    normalized += ' ';
    auto contains = [&](std::initializer_list<char const*> phrases)
    {
        return std::any_of(phrases.begin(), phrases.end(),
                           [&](char const* phrase) { return normalized.find(phrase) != std::string::npos; });
    };
    result.Casual = contains({" yo ", " heya ", " dude ", " lol ", " yep ", " nah ", " gonna ", " kinda ", " gotta ",
                             " oof ", " welp ", " rip ", " gg ", " np ", " grats "});
    result.Familiar = contains({" see you again ", " seen you before ", " remember you ", " been a while ",
                                " familiar face ", " crossed paths before "});
    result.Collective = contains({" everyone ", " you all ", " folks ", " everybody ", " anyone ", " anybody ",
                                  " who else ", " who wants ", " few people ", " other people "});
    result.Energetic = english.find('!') != std::string::npos;
    result.Question = english.find('?') != std::string::npos;
    return result;
}

std::uint32_t SpeechTraits::Weight(BotPersona const& persona, BotMood const& mood, DialogueContext const& context) const
{
    if ((context.RequireQuestion && !Question) ||
        (Familiar && (!context.KnownPartner || context.Affinity < 0)) || (Collective && !context.PublicAudience) ||
        (Casual && persona.SpeechStyle >= 80) || (Energetic && (mood.Valence < -35 || mood.Stress >= 75)))
        return 0;
    std::uint32_t weight = 10;
    bool concise = persona.SpeechStyle < 45;
    weight += (concise ? Bytes <= 45 : Bytes > 45) ? 25 : 0;
    if (Casual)
        weight += (100 - persona.SpeechStyle) / 2;
    if (Energetic)
        weight += persona.Sociability / 5 + std::max<int>(0, mood.Valence) / 5;
    if (Familiar)
        weight += std::max<int>(0, context.Affinity) / 2;
    if (Question)
        weight += persona.Sociability / 10 + persona.Patience / 10;
    else if (context.PreferStatement)
        weight += 25;
    return weight;
}

std::size_t SelectSpeech(std::vector<SpeechTraits> const& profiles, BotPersona const& persona, BotMood const& mood,
                         DialogueContext const& context, std::deque<std::uint32_t> const& recent,
                         std::uint32_t selector)
{
    std::vector<std::uint32_t> weights;
    weights.reserve(profiles.size());
    std::uint32_t eligible = 0;
    for (auto const& profile : profiles)
    {
        auto weight = profile.Weight(persona, mood, context);
        weights.push_back(weight);
        if (weight)
            ++eligible;
    }
    if (!eligible)
        throw std::runtime_error("ADVAI no context-eligible speech in catalog");
    std::uint32_t remaining = std::min<std::uint32_t>(MAX_RECENT_LINES, eligible - 1);
    for (auto previous = recent.rbegin(); previous != recent.rend() && remaining; ++previous)
        for (std::size_t index = 0; index < profiles.size(); ++index)
            if (weights[index] && profiles[index].Id == *previous)
            {
                weights[index] = 0;
                --remaining;
                break;
            }
    std::uint32_t total = 0;
    for (auto weight : weights)
        total += weight;
    selector %= total;
    for (std::size_t index = 0; index < weights.size(); ++index)
    {
        if (selector < weights[index])
            return index;
        selector -= weights[index];
    }
    throw std::runtime_error("ADVAI speech weight selection failed");
}
}  // namespace Advai
