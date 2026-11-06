/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_CONSENT_H
#define PLAYERBOTS_ADVAI_CONSENT_H

#include <algorithm>
#include <array>
#include <string_view>

namespace Advai
{
inline bool AcceptsInvitation(std::string_view normalized)
{
    constexpr std::array answers = {"yes",        "yes please",  "yes thanks",  "sure",       "sure thing", "okay",
                                    "ok",         "sounds good", "let s do it", "lets do it", "yep",        "yeah",
                                    "absolutely", "why not",     "i m in",      "count me in"};
    return std::find(answers.begin(), answers.end(), normalized) != answers.end();
}

inline bool DeclinesInvitation(std::string_view normalized)
{
    constexpr std::array answers = {"no",          "nope",      "no thanks",     "no thank you", "not now",
                                    "maybe later", "not today", "sorry not now", "another time", "maybe another time",
                                    "i m busy",    "i am busy", "not right now"};
    return std::find(answers.begin(), answers.end(), normalized) != answers.end();
}
}  // namespace Advai

#endif
