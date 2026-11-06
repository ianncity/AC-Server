/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#include "AdvaiLlm.h"

#include <algorithm>
#include <ctime>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "AdvaiMgr.h"
#include "AdvaiProfession.h"
#include "AscensionCultist.h"
#include "ChatHelper.h"
#include "DBCStores.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldSession.h"

namespace Advai
{
bool LlmFriendlySpell(Player* bot, std::uint32_t id)
{
    auto* spell = sSpellMgr->GetSpellInfo(id);
    auto known = bot->GetSpellMap().find(id);
    if (!spell || known == bot->GetSpellMap().end() || !known->second ||
        known->second->State == PLAYERSPELL_REMOVED || !known->second->Active ||
        !known->second->IsInSpec(bot->GetActiveSpec()) || !spell->IsPositive() || spell->IsPassive())
        return false;
    // Requests cast on the speaker must have a friendly unit target, not only self.
    if (!(spell->GetExplicitTargetMask() & (TARGET_FLAG_UNIT_ALLY | TARGET_FLAG_UNIT_PARTY | TARGET_FLAG_UNIT_RAID)))
        return false;
    bool nativeCustom = (bot->getClass() == CLASS_CULTIST && spell->SpellFamilyName == 31) ||
                        (bot->getClass() == CLASS_NECROMANCER && spell->SpellFamilyName == 29);
    for (auto const& effect : spell->Effects)
        if (effect.Effect && effect.Effect != SPELL_EFFECT_APPLY_AURA && effect.Effect != SPELL_EFFECT_HEAL &&
            effect.Effect != SPELL_EFFECT_HEAL_PCT && effect.Effect != SPELL_EFFECT_DISPEL &&
            effect.Effect != SPELL_EFFECT_ENERGIZE && effect.Effect != SPELL_EFFECT_ENERGIZE_PCT &&
            !(nativeCustom && (effect.Effect == SPELL_EFFECT_DUMMY || effect.Effect == SPELL_EFFECT_SCRIPT_EFFECT ||
                              effect.Effect == SPELL_EFFECT_TRIGGER_SPELL)))
            return false;
    return true;
}

LlmRequest CaptureLlmRequest(PlayerbotAI* ai, Player* sender, BotState const& state,
                            std::uint32_t channel, std::string const& text,
                            std::string const& channelName, std::uint64_t thread, bool initiating)
{
    auto* bot = ai->GetBot();
    LlmRequest request;
    request.Bot = bot->GetGUID().GetCounter();
    request.Sender = sender->GetGUID().GetCounter();
    request.Revision = state.LlmRevision;
    request.Thread = thread;
    request.Channel = channel;
    request.ChannelName = channelName;
    request.Incoming = text;
    request.Human = sender->GetSession() && !sender->GetSession()->IsHeadless();
    request.Initiating = initiating;
    request.Expires = static_cast<Timestamp>(time(nullptr)) + sPlayerbotAIConfig.AdvaiLlmQueueSeconds;
    request.Map = bot->GetMapId();
    request.Zone = bot->GetZoneId();
    request.Activity = state.LifeGoal.Kind;
    auto const& persona = state.Persona;
    auto contact = state.Relationships.find(request.Sender);
    auto* race = sChrRacesStore.LookupEntry(bot->getRace());
    unsigned livingSummons = 0;
    if (bot->getClass() == CLASS_NECROMANCER)
        for (auto* unit : bot->m_Controlled)
            if (unit && unit->IsAlive() && unit->GetOwnerGUID() == bot->GetGUID())
                ++livingSummons;
    std::ostringstream context;
    context << "incoming speaker=" << LlmJsonString(sender->GetName()) << " text=" << LlmJsonString(text)
        << " initiative=" << initiating << '\n';
    context << "self name=" << LlmJsonString(bot->GetName()) << " level=" << unsigned(bot->GetLevel())
        << " class=" << LlmJsonString(state.Snapshot.ClassName) << " spec=" << LlmJsonString(state.Snapshot.SpecName)
        << " race=" << LlmJsonString(race ? race->name[0] : "") << " zone=" << LlmJsonString(state.Snapshot.ZoneName) << '\n';
    context << "persona social=" << unsigned(persona.Sociability) << " patience=" << unsigned(persona.Patience)
        << " generosity=" << unsigned(persona.Generosity) << " competitive=" << unsigned(persona.Competitiveness)
        << " risk=" << unsigned(persona.RiskTolerance) << " frugal=" << unsigned(persona.Frugality)
        << " skill=" << unsigned(persona.Skill) << " style=" << unsigned(persona.SpeechStyle) << '\n';
    context << "now goal=" << GoalPlanner::Name(state.LifeGoal.Kind)
        << " ambition=" << BotAmbition::Name(state.Cognition.Ambition.Kind)
        << " progress=" << state.Cognition.Ambition.Progress << " preferred=" << GoalPlanner::Name(persona.Preference)
        << " mood=" << state.Cognition.Mood.Valence << " stress=" << unsigned(state.Cognition.Mood.Stress)
        << " copper=" << bot->GetMoney() << '\n';
    context << "peer level=" << unsigned(sender->GetLevel())
        << " affinity=" << (contact == state.Relationships.end() ? 0 : contact->second.Affinity)
        << " encounters=" << (contact == state.Relationships.end() ? 0 : contact->second.Encounters)
        << " grouped=" << (bot->GetGroup() && bot->GetGroup() == sender->GetGroup())
        << " guildmate=" << (bot->GetGuildId() && bot->GetGuildId() == sender->GetGuildId()) << '\n';
    context << "needs food=" << state.Snapshot.NeedsFood << " drink=" << state.Snapshot.NeedsDrink
        << " repair=" << (state.Snapshot.Durability < 25) << " training=" << state.Snapshot.NeedsTraining
        << " bag_space=" << unsigned(state.Snapshot.FreeBagSlots) << '\n';
    if (state.LlmPartner == request.Sender)
    {
        auto begin = state.LlmHistory.size() > 2 ? state.LlmHistory.size() - 2 : 0;
        for (std::size_t i = begin; i < state.LlmHistory.size(); ++i)
            context << "recent " << state.LlmHistory[i].first << ' ' << LlmJsonString(state.LlmHistory[i].second) << '\n';
    }
    if (bot->GetMaxPower(POWER_MANA))
        context << "mana " << bot->GetPower(POWER_MANA) << '/' << bot->GetMaxPower(POWER_MANA) << '\n';
    if (bot->getClass() == CLASS_CULTIST)
        context << "resources insanity=" << bot->GetAuraCount(AscensionCultist::Insanity)
            << " void_runes=" << bot->GetAuraCount(AscensionCultist::VoidRune)
            << " madness=" << bot->GetAuraCount(AscensionCultist::Madness)
            << " black_blood=" << bot->GetAuraCount(AscensionCultist::BlackBlood) << '\n';
    if (bot->getClass() == CLASS_NECROMANCER)
        context << "living_owned_summons " << livingSummons << '\n';
    if (!state.Snapshot.QuestName.empty())
        context << "quest " << LlmJsonString(state.Snapshot.QuestName) << '\n';
    if (!state.Snapshot.GuildName.empty())
        context << "guild " << LlmJsonString(state.Snapshot.GuildName) << '\n';
    auto parsed = ChatRules::Parse(text);
    bool reference = state.LlmPartner == request.Sender && state.LlmFocusAt &&
        state.LlmFocusAt + 120 > static_cast<Timestamp>(time(nullptr)) && parsed.Items.empty() &&
        parsed.Spells.empty() && !parsed.TopicChange &&
        std::any_of(parsed.Tokens.begin(), parsed.Tokens.end(), [](auto const& token)
            { return token == "that" || token == "it" || token == "same" || token == "again" || token == "those"; });
    auto addItem = [&](std::uint32_t id)
    {
        if (request.Items.size() >= 8 || std::find(request.Items.begin(), request.Items.end(), id) != request.Items.end())
            return;
        auto* proto = sObjectMgr->GetItemTemplate(id);
        if (!proto)
            return;
        request.Items.push_back(id);
        context << "item " << id << ' ' << LlmJsonString(proto->Name1)
                << " owned=" << bot->GetItemCount(id, false);
        if (reference && id == state.LlmItemFocus)
            context << " previous_subject=1";
        context << '\n';
    };
    for (auto id : parsed.Items)
        addItem(id);
    if (reference && state.LlmItemFocus)
        addItem(state.LlmItemFocus);
    auto inventory = ai->GetInventoryAndEquippedItems();
    auto relevant = [&](std::string const& name)
    {
        auto tokens = ChatRules::Parse(name).Tokens;
        return std::any_of(tokens.begin(), tokens.end(), [&](auto const& token)
            { return token.size() >= 4 && std::find(parsed.Tokens.begin(), parsed.Tokens.end(), token) != parsed.Tokens.end(); });
    };
    for (auto* item : inventory)
        if (item && !item->IsEquipped() && relevant(item->GetTemplate()->Name1))
            addItem(item->GetEntry());
    for (auto const& [id, learned] : bot->GetSpellMap())
        if (learned && learned->State != PLAYERSPELL_REMOVED && learned->Active && learned->IsInSpec(bot->GetActiveSpec()))
            if (auto* spell = sSpellMgr->GetSpellInfo(id))
                for (auto const& effect : spell->Effects)
                    if (effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.ItemType)
                        if (auto* product = sObjectMgr->GetItemTemplate(effect.ItemType);
                            product && relevant(product->Name1) && KnownRecipeSpell(bot, product->ItemId))
                            addItem(product->ItemId);
    auto addSpell = [&](std::uint32_t id)
    {
        if (!LlmFriendlySpell(bot, id) ||
            std::find(request.Spells.begin(), request.Spells.end(), id) != request.Spells.end())
            return;
        auto* spell = sSpellMgr->GetSpellInfo(id);
        for (auto& selected : request.Spells)
            if (auto* existing = sSpellMgr->GetSpellInfo(selected);
                existing && std::string(existing->SpellName[0]) == spell->SpellName[0])
            {
                bool explicitRank = std::find(parsed.Spells.begin(), parsed.Spells.end(), selected) != parsed.Spells.end();
                if (!explicitRank && spell->SpellLevel > existing->SpellLevel)
                    selected = id;
                return;
            }
        if (request.Spells.size() >= 8)
            return;
        request.Spells.push_back(id);
    };
    for (auto id : parsed.Spells)
        addSpell(id);
    if (reference && state.LlmSpellFocus)
        addSpell(state.LlmSpellFocus);
    for (auto const& [id, learned] : bot->GetSpellMap())
        if (learned && learned->State != PLAYERSPELL_REMOVED && learned->Active && learned->IsInSpec(bot->GetActiveSpec()))
            if (auto* spell = sSpellMgr->GetSpellInfo(id); spell && relevant(spell->SpellName[0]))
                addSpell(id);
    for (auto const& [id, learned] : bot->GetSpellMap())
        if (learned && learned->State != PLAYERSPELL_REMOVED && learned->Active && learned->IsInSpec(bot->GetActiveSpec()))
            addSpell(id);
    for (auto id : request.Spells)
    {
        context << "spell " << id << ' ' << LlmJsonString(sSpellMgr->GetSpellInfo(id)->SpellName[0]);
        if (reference && state.LlmSpellFocus)
            if (auto* previous = sSpellMgr->GetSpellInfo(state.LlmSpellFocus);
                previous && std::string(previous->SpellName[0]) == sSpellMgr->GetSpellInfo(id)->SpellName[0])
                context << " previous_subject=1";
        context << '\n';
    }
    for (auto* item : inventory)
        if (item && !item->IsEquipped())
            addItem(item->GetEntry());
    static char const* const experienceNames[] = {"conversation", "assistance", "shared quest", "trade",
        "victory", "wipe", "death", "broken commitment", "guild"};
    unsigned events = 0;
    for (auto it = state.Cognition.Memory.Recent.rbegin(); it != state.Cognition.Memory.Recent.rend() && events < 3; ++it)
        if (it->Other == request.Sender && it->Kind != SocialEventKind::Conversation &&
            static_cast<unsigned>(it->Kind) < std::size(experienceNames))
        {
            context << "experience " << experienceNames[static_cast<unsigned>(it->Kind)]
                << " positive=" << it->Positive << '\n';
            ++events;
        }
    context << "profession_skill " << state.Snapshot.ProfessionSkill << '\n';
    if (state.LlmPartner == request.Sender && state.LlmHistory.size() > 2)
        for (std::size_t i = 0; i < state.LlmHistory.size() - 2; ++i)
            context << "older " << state.LlmHistory[i].first << ' ' << LlmJsonString(state.LlmHistory[i].second) << '\n';
    request.Context = context.str();
    return request;
}

std::string DescribeLlmSpells(PlayerbotAI* ai)
{
    std::string result = "Some spells I know: ";
    unsigned count = 0;
    std::unordered_set<std::string> names;
    for (auto const& [id, learned] : ai->GetBot()->GetSpellMap())
        if (LlmFriendlySpell(ai->GetBot(), id))
        {
            auto* spell = sSpellMgr->GetSpellInfo(id);
            if (!names.insert(spell->SpellName[0]).second)
                continue;
            auto entry = (count ? ", " : "") + ChatHelper::FormatSpell(spell);
            if (result.size() + entry.size() + 4 > MAX_CHAT_BYTES || count >= 3)
                return result + " ...";
            result += entry;
            ++count;
        }
    return count ? result + '.' : "I don't know a friendly spell I can offer.";
}

std::string DescribeLlmInventory(PlayerbotAI* ai)
{
    std::string result = "In my bags: ";
    std::unordered_set<std::uint32_t> seen;
    std::unordered_map<std::uint32_t, std::uint32_t> carried;
    auto inventory = ai->GetInventoryAndEquippedItems();
    for (auto* item : inventory)
        if (item && !item->IsEquipped())
            carried[item->GetEntry()] += item->GetCount();
    unsigned count = 0;
    for (auto* item : inventory)
        if (item && !item->IsEquipped() && seen.insert(item->GetEntry()).second)
        {
            auto entry = (count ? ", " : "") + std::to_string(carried[item->GetEntry()]) +
                         "x " + ai->GetChatHelper()->FormatItem(item->GetTemplate());
            if (result.size() + entry.size() + 4 > MAX_CHAT_BYTES || count >= 3)
                return result + " ...";
            result += entry;
            ++count;
        }
    return count ? result + '.' : "I don't have anything in my bags.";
}
}
