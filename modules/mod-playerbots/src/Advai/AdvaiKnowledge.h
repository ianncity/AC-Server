/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_KNOWLEDGE_H
#define PLAYERBOTS_ADVAI_KNOWLEDGE_H

#include <cstdint>
#include <optional>
#include <string>

#include "AdvaiCatalog.h"
#include "AdvaiModel.h"

class Player;

namespace Advai
{
std::string LocalizedFacts(CatalogHandle const& catalog, std::uint32_t locale, Intent intent,
                           BotActivitySnapshot const& snapshot, BotLifeGoal const& goal,
                           ParsedMessage const* question = nullptr, Player* bot = nullptr, Player* sender = nullptr);
std::optional<std::string> QuestFacts(CatalogHandle const& catalog, std::uint32_t locale, Player* bot,
                                      std::uint32_t questId, QuestSubject subject);
std::optional<std::string> QuestFacts(Player* bot, std::uint32_t questId,
                                      QuestSubject subject = QuestSubject::Progress);
std::optional<std::string> EntityFacts(CatalogHandle const& catalog, std::uint32_t locale, Player* bot,
                                     Intent topic, std::uint32_t entity, ParsedMessage const* question = nullptr);
std::string ProfessionFacts(CatalogHandle const& catalog, std::uint32_t locale, Player* bot,
                            ParsedMessage const& question, std::uint32_t focus = 0, std::uint32_t craftSpell = 0);
std::uint32_t ProfessionSubject(ParsedMessage const& question);
std::string SpecializationName(Player* bot);
}  // namespace Advai

#endif
