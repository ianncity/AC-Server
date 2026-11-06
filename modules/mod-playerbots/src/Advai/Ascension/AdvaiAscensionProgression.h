/*
 * This file is part of mod-playerbots; GNU GPL v2 or later.
 */
#ifndef PLAYERBOTS_ADVAI_ASCENSION_PROGRESSION_H
#define PLAYERBOTS_ADVAI_ASCENSION_PROGRESSION_H

#include "Define.h"

class Player;
class PlayerbotAI;

#include <string>

namespace Advai::Ascension
{
uint32 Specialization(Player* player);
void SeedClass(Player* player);
uint32 AdvanceClass(Player* player, uint32 maximum = 8);
bool Act(PlayerbotAI* ai);
std::string Explain(PlayerbotAI* ai);
std::string CastReport();
void ResetCastReport();
}  // namespace Advai::Ascension

#endif
