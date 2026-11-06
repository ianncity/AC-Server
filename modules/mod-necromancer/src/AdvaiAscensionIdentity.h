#ifndef ADVAI_ASCENSION_IDENTITY_H
#define ADVAI_ASCENSION_IDENTITY_H

#include "Player.h"
#include "SharedDefines.h"

namespace Advai::Ascension
{
// Bound to the definitions in the playerbots module (AdvaiAscensionRegistration.cpp)
// so the class runtime and the bot stack agree on a single implementation.
bool Enabled();
bool CanThink(Player* player);

inline bool IsCultist(Player const* player)
{
    return player && player->getClass() == CLASS_CULTIST;
}

inline bool IsNecromancer(Player const* player)
{
    return player && player->getClass() == CLASS_NECROMANCER;
}
} // namespace Advai::Ascension

#endif
