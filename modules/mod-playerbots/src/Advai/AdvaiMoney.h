/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_MONEY_H
#define PLAYERBOTS_ADVAI_MONEY_H

#include <charconv>
#include <cstdint>
#include <string>
#include <vector>

namespace Advai
{
struct PriceQuote
{
    bool Found = false;
    bool Valid = true;
    std::uint32_t Copper = 0;
};

inline PriceQuote ParsePrice(std::vector<std::string> const& tokens)
{
    PriceQuote result;
    std::uint64_t total = 0;
    std::uint32_t previousUnit = 100000;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        std::string token = tokens[i];
        if (token.empty() || (token.front() != '-' && (token.front() < '0' || token.front() > '9')))
            continue;
        if (i + 1 < tokens.size())
        {
            auto const& next = tokens[i + 1];
            if (next == "gold" || next == "g" || next == "silver" || next == "s" || next == "copper" || next == "c")
            {
                token += next.front();
                ++i;
            }
        }
        if (token.find_first_of("gsc") == std::string::npos)
            continue;
        result.Found = true;
        std::size_t cursor = 0;
        while (cursor < token.size())
        {
            std::uint64_t whole = 0, fraction = 0, denominator = 1;
            auto begin = token.data() + cursor;
            auto end = token.data() + token.size();
            auto parsed = std::from_chars(begin, end, whole);
            if (parsed.ec != std::errc{} || parsed.ptr == begin)
                return {true, false, 0};
            cursor = parsed.ptr - token.data();
            if (cursor < token.size() && token[cursor] == '.')
            {
                auto start = ++cursor;
                while (cursor < token.size() && token[cursor] >= '0' && token[cursor] <= '9')
                {
                    if (cursor - start >= 4)
                        return {true, false, 0};
                    fraction = fraction * 10 + token[cursor++] - '0';
                    denominator *= 10;
                }
                if (cursor == start)
                    return {true, false, 0};
            }
            if (cursor >= token.size())
                return {true, false, 0};
            char unit = token[cursor++];
            std::uint32_t multiplier = unit == 'g' ? 10000 : unit == 's' ? 100 : unit == 'c' ? 1 : 0;
            if (!multiplier || multiplier >= previousUnit || whole > 2147483647ULL / multiplier ||
                fraction * multiplier % denominator)
                return {true, false, 0};
            previousUnit = multiplier;
            total += whole * multiplier + fraction * multiplier / denominator;
            if (total > 2147483647ULL)
                return {true, false, 0};
        }
    }
    result.Copper = static_cast<std::uint32_t>(total);
    return result;
}
}  // namespace Advai

#endif
