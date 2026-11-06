/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_ASCENSION_IDENTITY_H
#define PLAYERBOTS_ADVAI_ASCENSION_IDENTITY_H

#include "AdvaiMgr.h"
#include "Player.h"

namespace Advai::Ascension
{
bool Enabled();
bool CanThink(Player* player);
void ValidateContent();

inline bool HasIdentity(Player const* player, ClassProfile profile, uint8 classId)
{
    if (!Enabled() || !player || player->getClass() != classId)
        return false;
    auto& manager = Manager::Instance();
    return !manager.Applies(player) || manager.Profile(player->GetGUID().GetCounter()) == profile;
}

inline bool IsBotCultist(Player const* player) { return HasIdentity(player, ClassProfile::Cultist, CLASS_CULTIST); }

inline bool IsBotNecromancer(Player const* player)
{
    return HasIdentity(player, ClassProfile::Necromancer, CLASS_NECROMANCER);
}
}  // namespace Advai::Ascension

#endif
