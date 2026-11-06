/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_ADVAI_PVP_H
#define PLAYERBOTS_ADVAI_PVP_H

#include "AdvaiCatalog.h"
#include "AdvaiModel.h"

class Player;
class PlayerbotAI;
class Unit;

namespace Advai
{
bool IsPvpIntent(Intent topic);
bool VisiblePvpEnemy(Player* bot, Unit* target, float range = 45.0f);
Unit* ChoosePvpTarget(PlayerbotAI* ai, std::vector<Unit*> const& candidates, Unit* current);
Player* PvpRegroupAnchor(PlayerbotAI* ai);
Player* PvpObjectiveAnchor(PlayerbotAI* ai);
Unit* PvpRequestTarget(PlayerbotAI* ai, Player* sender, ParsedMessage const& message, Intent topic);
std::optional<std::string> PvpFacts(CatalogHandle const& catalog, std::uint32_t locale,
                                  PlayerbotAI* ai, Intent topic, BotId observed = 0);
}
#endif
