/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PlayerbotTextMgr.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "AdvaiCatalog.h"
#include "AdvaiDialogue.h"
#include "AdvaiFactDefinitions.h"
#include "AdvaiModel.h"
#include "DatabaseEnv.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotsDatabase.h"
#include "QueryResult.h"  // Required due to a poor implementation by AC
#include "Random.h"
#include "World.h"
#include "WorldSessionMgr.h"

namespace Advai
{
struct SpeechData
{
    std::map<std::string, std::vector<BotTextEntry>> Entries;
    std::map<std::string, std::vector<SpeechTraits>> Profiles;
    std::unordered_map<std::string, std::string> LiteralKeys;
    std::unordered_map<std::string, std::size_t> FactParameters;
};
struct SpeechIndex
{
    static std::atomic<std::shared_ptr<SpeechData const>> Published;
};
std::atomic<std::shared_ptr<SpeechData const>> SpeechIndex::Published;
}  // namespace Advai

PlayerbotTextMgr::PlayerbotTextMgr() : _advaiSpeech(std::make_unique<Advai::SpeechIndex>())
{
    for (uint8 i = 0; i < TOTAL_LOCALES; ++i)
        botTextLocalePriority[i] = 0;
}

PlayerbotTextMgr::~PlayerbotTextMgr() = default;

namespace
{
void ValidateFactData(Advai::SpeechData& catalog)
{
    for (auto const& definition : Advai::FACT_DEFINITIONS)
    {
        std::string key = "advai_fact_" + std::string(definition.Key);
        auto entries = catalog.Entries.find(key);
        if (entries == catalog.Entries.end() || entries->second.size() != 1)
            throw std::runtime_error("ADVAI factual catalog needs exactly one template: " + key);
        auto const& entry = entries->second.front();
        for (auto locale : {Advai::TEXT_LOCALE})
        {
            auto found = entry.m_text.find(locale);
            if (found == entry.m_text.end() || found->second.empty() || found->second.size() > 200)
                throw std::runtime_error("ADVAI missing or oversized factual template: " + key + " locale " +
                                         localeNames[locale]);
            auto const& text = found->second;
            uint32 mask = 0;
            for (std::size_t index = 0; index < text.size(); ++index)
                if (text[index] == '{')
                {
                    if (index + 2 >= text.size() || text[index + 1] < '0' || text[index + 1] > '3' ||
                        text[index + 2] != '}')
                        throw std::runtime_error("ADVAI invalid factual placeholder: " + key);
                    uint32 bit = 1U << (text[index + 1] - '0');
                    if (mask & bit)
                        throw std::runtime_error("ADVAI repeated factual placeholder: " + key);
                    mask |= bit;
                    index += 2;
                }
                else if (text[index] == '}')
                    throw std::runtime_error("ADVAI unmatched factual placeholder: " + key);
            if (mask != (1U << definition.Parameters) - 1)
                throw std::runtime_error("ADVAI factual placeholder mismatch: " + key);
        }
        catalog.FactParameters.emplace(key, definition.Parameters);
        if (!definition.Parameters && !catalog.LiteralKeys.emplace(entry.m_text.at(0), key).second)
            throw std::runtime_error("ADVAI ambiguous canonical factual text: " + key);
    }
    for (auto const& [key, entries] : catalog.Entries)
        if (key.starts_with("advai_fact_") && !catalog.FactParameters.count(key))
            throw std::runtime_error("ADVAI unknown factual template: " + key);
}

void ValidateSpeechData(Advai::SpeechData const& catalog)
{
    std::unordered_set<std::string> required;
    for (uint32 intent = 0; intent < static_cast<uint32>(Advai::Intent::Count); ++intent)
    {
        std::string key = std::string("advai_") + Advai::ChatRules::Name(static_cast<Advai::Intent>(intent));
        required.insert(key);
        auto found = catalog.Entries.find(key);
        if (found == catalog.Entries.end() || found->second.size() < Advai::MIN_TEXT_VARIANTS)
            throw std::runtime_error("ADVAI text catalog is incomplete: " + key);
        for (auto locale : {Advai::TEXT_LOCALE})
        {
            std::unordered_set<std::string> unique;
            for (auto const& entry : found->second)
            {
                auto translated = entry.m_text.find(locale);
                if (translated == entry.m_text.end() || translated->second.empty() ||
                    translated->second.size() > Advai::MAX_CHAT_BYTES || !unique.insert(translated->second).second)
                    throw std::runtime_error("ADVAI missing, duplicate, or oversized text: " + key + " locale " +
                                             localeNames[locale]);
                auto const& source = entry.m_text.at(0);
                if (std::count(source.begin(), source.end(), '%') !=
                    std::count(translated->second.begin(), translated->second.end(), '%'))
                    throw std::runtime_error("ADVAI translated placeholder count mismatch: " + key);
                auto placeholder = translated->second.find('%');
                while (placeholder != std::string::npos)
                {
                    if (translated->second.compare(placeholder, 6, "%facts") != 0 ||
                        (placeholder + 6 < translated->second.size() &&
                         (std::isalnum(static_cast<unsigned char>(translated->second[placeholder + 6])) ||
                          translated->second[placeholder + 6] == '_')))
                        throw std::runtime_error("ADVAI invalid text placeholder: " + key);
                    placeholder = translated->second.find('%', placeholder + 6);
                }
            }
        }
        auto profiles = catalog.Profiles.find(key);
        if (profiles == catalog.Profiles.end())
            throw std::runtime_error("ADVAI required speech index not loaded: " + key);
        Advai::BotPersona persona;
        persona.SpeechStyle = 100;
        Advai::BotMood mood;
        mood.Valence = -100;
        mood.Stress = 100;
        Advai::DialogueContext context;
        context.KnownPartner = intent == static_cast<uint32>(Advai::Intent::FriendRemembered);
        context.PublicAudience = intent == static_cast<uint32>(Advai::Intent::Progress) ||
                                 intent == static_cast<uint32>(Advai::Intent::GroupOffer);
        Advai::SelectSpeech(profiles->second, persona, mood, context, {}, 0);
    }
    for (auto const& [key, entries] : catalog.Entries)
        if (!key.starts_with("advai_fact_") && !required.count(key))
            throw std::runtime_error("ADVAI unknown conversation outcome: " + key);
}
}  // namespace

void PlayerbotTextMgr::ValidateAdvaiTexts() const
{
    auto catalog = _advaiSpeech->Published.load();
    if (!catalog)
        throw std::runtime_error("ADVAI required speech index is disabled");
    ValidateSpeechData(*catalog);
}

std::string PlayerbotTextMgr::GetAdvaiText(std::string const& key, uint32 locale, uint32 selector,
                                           std::deque<uint32> const& recent, uint32& variant,
                                           Advai::BotPersona const& persona, Advai::BotMood const& mood,
                                           Advai::DialogueContext const& context) const
{
    auto line =
        Advai::SelectCatalogSpeech(Advai::CurrentCatalog(), key, locale, selector, recent, persona, mood, context);
    variant = line.Variant;
    return std::move(line.Text);
}

Advai::CatalogHandle Advai::CurrentCatalog()
{
    auto catalog = SpeechIndex::Published.load();
    if (!catalog)
        throw std::runtime_error("ADVAI speech index is disabled");
    return catalog;
}

Advai::SpeechLine Advai::SelectCatalogSpeech(CatalogHandle const& catalog, std::string const& key, std::uint32_t locale,
                                             std::uint32_t selector, std::deque<std::uint32_t> const& recent,
                                             BotPersona const& persona, BotMood const& mood,
                                             DialogueContext const& context)
{
    if (!catalog)
        throw std::runtime_error("ADVAI speech selection requires a validated catalog snapshot");
    if (locale != TEXT_LOCALE)
        throw std::runtime_error("ADVAI supports English speech only");
    auto found = catalog->Entries.find(key);
    if (found == catalog->Entries.end() || found->second.empty())
        throw std::runtime_error("ADVAI required text not loaded: " + key);
    auto const& entries = found->second;
    auto profiles = catalog->Profiles.find(key);
    if (profiles == catalog->Profiles.end() || profiles->second.size() != entries.size())
        throw std::runtime_error("ADVAI required speech index not loaded: " + key);
    auto index = Advai::SelectSpeech(profiles->second, persona, mood, context, recent, selector);
    auto text = entries[index].m_text.find(locale);
    if (text == entries[index].m_text.end() || text->second.empty())
        throw std::runtime_error("ADVAI required locale not loaded: " + key);
    return {text->second, entries[index].m_text.at(0), profiles->second[index].Id, catalog};
}

std::string Advai::CatalogFact(CatalogHandle const& catalog, std::string const& key, std::uint32_t locale,
                               std::initializer_list<std::string> values)
{
    if (!catalog)
        throw std::runtime_error("ADVAI factual rendering requires a validated catalog snapshot");
    if (locale != TEXT_LOCALE)
        throw std::runtime_error("ADVAI supports English factual text only");
    std::string name = "advai_fact_" + key;
    auto parameters = catalog->FactParameters.find(name);
    if (parameters == catalog->FactParameters.end() || parameters->second != values.size())
        throw std::runtime_error("ADVAI missing factual template or invalid arguments: " + name);
    auto const& localized = catalog->Entries.at(name).front().m_text;
    auto found = localized.find(locale);
    if (found == localized.end() || found->second.empty())
        throw std::runtime_error("ADVAI missing factual locale: " + name);
    std::string result;
    auto const& text = found->second;
    for (std::size_t index = 0; index < text.size(); ++index)
        if (text[index] == '{')
        {
            result += *(values.begin() + (text[index + 1] - '0'));
            index += 2;
        }
        else
            result += text[index];
    return result;
}

std::string Advai::CatalogLiteral(CatalogHandle const& catalog, std::string const& english, std::uint32_t locale)
{
    if (!catalog)
        throw std::runtime_error("ADVAI factual rendering requires a validated catalog snapshot");
    auto key = catalog->LiteralKeys.find(english);
    if (key == catalog->LiteralKeys.end())
        throw std::runtime_error("ADVAI canonical factual text is missing from the catalog: " + english);
    return CatalogFact(catalog, key->second.substr(11), locale);
}

void PlayerbotTextMgr::replaceAll(std::string& str, std::string const& from, std::string const& to)
{
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

void PlayerbotTextMgr::LoadBotTexts()
{
    LOG_INFO("playerbots", "Loading playerbots texts...");

    uint32 count = 0;
    auto speech = sPlayerbotAIConfig.AdvaiEnabled ? std::make_shared<Advai::SpeechData>() : nullptr;
    if (PreparedQueryResult result =
            PlayerbotsDatabase.Query(PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_TEXT)))
    {
        do
        {
            std::map<uint32, std::string> text;
            Field* fields = result->Fetch();
            std::string name = fields[0].Get<std::string>();
            text[0] = fields[1].Get<std::string>();
            uint8 sayType = fields[2].Get<uint8>();
            uint8 replyType = fields[3].Get<uint8>();
            for (uint8 i = 1; i < TOTAL_LOCALES; ++i)
            {
                text[i] = fields[i + 3].Get<std::string>();
            }

            BotTextEntry entry(name, std::move(text), sayType, replyType);
            if (speech && name.starts_with("advai_"))
                speech->Entries[name].push_back(entry);
            botTexts[name].push_back(std::move(entry));
            ++count;
        } while (result->NextRow());
    }

    if (speech)
    {
        std::unordered_set<uint32> ids;
        for (auto const& [key, entries] : speech->Entries)
        {
            if (key.starts_with("advai_fact_"))
                continue;
            for (auto const& entry : entries)
            {
                auto english = entry.m_text.find(0);
                if (english == entry.m_text.end() || english->second.empty())
                    throw std::runtime_error("ADVAI speech index requires English source: " + key);
                auto profile = Advai::SpeechTraits::Analyze(key, english->second);
                if (!ids.insert(profile.Id).second)
                    throw std::runtime_error("ADVAI duplicate persistent speech identity: " + key);
                speech->Profiles[key].push_back(profile);
            }
        }
        ValidateFactData(*speech);
        ValidateSpeechData(*speech);
    }
    _advaiSpeech->Published.store(std::move(speech));

    LOG_INFO("playerbots", "{} playerbots texts loaded", count);
}

void PlayerbotTextMgr::LoadBotTextChance()
{
    if (botTextChance.empty())
    {
        PlayerbotsDatabasePreparedStatement* stmt = PlayerbotsDatabase.GetPreparedStatement(PLAYERBOTS_SEL_TEXT_CHANCE);
        PreparedQueryResult results = PlayerbotsDatabase.Query(stmt);
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                std::string name = fields[0].Get<std::string>();
                uint32 probability = fields[1].Get<uint32>();

                botTextChance[name] = probability;
            } while (results->NextRow());
        }
    }
}

// general texts

std::string PlayerbotTextMgr::GetBotText(std::string name)
{
    if (botTexts.empty())
    {
        LOG_ERROR("playerbots", "Can't get bot text {}! No bots texts loaded!", name);
        return "";
    }

    if (botTexts[name].empty())
    {
        LOG_ERROR("playerbots", "Can't get bot text {}! No bots texts for this name!", name);
        return "";
    }

    std::vector<BotTextEntry>& list = botTexts[name];
    BotTextEntry textEntry = list[urand(0, list.size() - 1)];
    return !textEntry.m_text[GetLocalePriority()].empty() ? textEntry.m_text[GetLocalePriority()] : textEntry.m_text[0];
}

std::string PlayerbotTextMgr::GetBotText(std::string name, std::map<std::string, std::string> placeholders)
{
    std::string botText = GetBotText(name);
    if (botText.empty())
        return "";

    for (std::map<std::string, std::string>::iterator i = placeholders.begin(); i != placeholders.end(); ++i)
        replaceAll(botText, i->first, i->second);

    return botText;
}

std::string PlayerbotTextMgr::GetBotTextOrDefault(std::string name, std::string defaultText,
    std::map<std::string, std::string> placeholders)
{
    std::string botText = GetBotText(name, placeholders);
    if (botText.empty())
    {
        for (std::map<std::string, std::string>::iterator i = placeholders.begin(); i != placeholders.end(); ++i)
        {
            replaceAll(defaultText, i->first, i->second);
        }
        return defaultText;
    }

    return botText;
}

// chat replies

std::string PlayerbotTextMgr::GetBotText(ChatReplyType replyType, std::map<std::string, std::string> placeholders)
{
    if (botTexts.empty())
    {
        LOG_ERROR("playerbots", "Can't get bot text reply {}! No bots texts loaded!", replyType);
        return "";
    }
    if (botTexts["reply"].empty())
    {
        LOG_ERROR("playerbots", "Can't get bot text reply {}! No bots texts replies!", replyType);
        return "";
    }

    std::vector<BotTextEntry>& list = botTexts["reply"];
    std::vector<BotTextEntry> proper_list;
    for (auto text : list)
    {
        if (text.m_replyType == replyType)
            proper_list.push_back(text);
    }

    if (proper_list.empty())
        return "";

    BotTextEntry textEntry = proper_list[urand(0, proper_list.size() - 1)];
    std::string botText =
        !textEntry.m_text[GetLocalePriority()].empty() ? textEntry.m_text[GetLocalePriority()] : textEntry.m_text[0];
    for (auto& placeholder : placeholders)
        replaceAll(botText, placeholder.first, placeholder.second);

    return botText;
}

std::string PlayerbotTextMgr::GetBotText(ChatReplyType replyType, std::string name)
{
    std::map<std::string, std::string> placeholders;
    placeholders["%s"] = name;

    return GetBotText(replyType, placeholders);
}

// probabilities

bool PlayerbotTextMgr::rollTextChance(std::string name)
{
    if (!botTextChance[name])
        return true;

    return urand(0, 100) < botTextChance[name];
}

bool PlayerbotTextMgr::GetBotText(std::string name, std::string& text)
{
    if (!rollTextChance(name))
        return false;

    text = GetBotText(name);
    return !text.empty();
}

bool PlayerbotTextMgr::GetBotText(std::string name, std::string& text, std::map<std::string, std::string> placeholders)
{
    if (!rollTextChance(name))
        return false;

    text = GetBotText(name, placeholders);
    return !text.empty();
}

void PlayerbotTextMgr::AddLocalePriority(uint32 locale)
{
    if (locale >= TOTAL_LOCALES)
    {
        LOG_WARN("playerbots", "Ignoring locale {} for bot texts because it exceeds TOTAL_LOCALES ({})", locale, TOTAL_LOCALES - 1);
        return;
    }

    botTextLocalePriority[locale]++;
}

uint32 PlayerbotTextMgr::GetLocalePriority()
{
    // if no real players online, reset top locale
    uint32 const activeSessions = sWorldSessionMgr->GetActiveSessionCount();
    if (!activeSessions)
    {
        ResetLocalePriority();
        return 0;
    }

    uint32 topLocale = 0;
    for (uint8 i = 0; i < TOTAL_LOCALES; ++i)
    {
        if (botTextLocalePriority[i] > botTextLocalePriority[topLocale])
            topLocale = i;
    }

    return topLocale;
}

void PlayerbotTextMgr::ResetLocalePriority()
{
    for (uint8 i = 0; i < TOTAL_LOCALES; ++i)
    {
        botTextLocalePriority[i] = 0;
    }
}
