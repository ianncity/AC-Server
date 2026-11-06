/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_ADVAI_LLM_H
#define PLAYERBOTS_ADVAI_LLM_H

#include "AdvaiModel.h"

class Player;
class PlayerbotAI;

namespace Advai
{
constexpr std::uint32_t LLM_CONTEXT_TOKENS = 1024;
constexpr std::uint32_t LLM_SLOT_TOKENS = 2048;
constexpr std::uint32_t LLM_OUTPUT_TOKENS = 160;

struct BotState;
struct LlmRequest
{
    BotId Bot = 0;
    BotId Sender = 0;
    std::uint64_t Revision = 0;
    std::uint64_t Thread = 0;
    std::uint32_t Channel = 0;
    Timestamp Expires = 0;
    std::uint64_t SubmittedMs = 0;  // game clock when the request was sent
    std::uint32_t TypingMs = 0;     // how long the bot wants to "type" before the reply may appear
    bool Human = true;
    bool Initiating = false;
    std::string ChannelName;
    std::string Incoming;
    std::string Context;
    std::uint32_t ContextTokens = 0;
    std::uint32_t PromptTokens = 0;
    bool ContextTruncated = false;
    std::vector<std::uint32_t> Items;
    std::vector<std::uint32_t> Spells;
    std::uint32_t Map = 0;
    std::uint32_t Zone = 0;
    Goal Activity = Goal::None;
};

struct LlmResult
{
    LlmRequest Request;
    std::string Action;
    std::string Reply;
    std::uint32_t Entity = 0;
    std::string Error;
};

std::string LlmJsonString(std::string const& value);
void InitializeLlm();
bool SubmitLlm(LlmRequest request);
std::vector<LlmResult> TakeLlmResults();
std::string LlmStatistics();
LlmRequest CaptureLlmRequest(PlayerbotAI* ai, Player* sender, BotState const& state,
                            std::uint32_t channel, std::string const& text,
                            std::string const& channelName, std::uint64_t thread, bool initiating = false);
std::string DescribeLlmInventory(PlayerbotAI* ai);
std::string DescribeLlmSpells(PlayerbotAI* ai);
bool LlmFriendlySpell(Player* bot, std::uint32_t spell);
}
#endif
