/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef PLAYERBOTS_ADVAI_BEHAVIOR_H
#define PLAYERBOTS_ADVAI_BEHAVIOR_H

#include <algorithm>
#include <cstdint>

namespace Advai
{
inline float TurnEase(float progress)
{
    progress = std::clamp(progress, 0.0f, 1.0f);
    return progress * progress * (3.0f - 2.0f * progress);
}

inline std::uint32_t PeerReplyStopChance(std::uint32_t messages)
{
    return messages < 3 ? 0 : messages >= 7 ? 100 : (messages - 2) * 20;
}
}  // namespace Advai
#endif
