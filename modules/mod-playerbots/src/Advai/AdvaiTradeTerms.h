/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_TRADE_TERMS_H
#define PLAYERBOTS_ADVAI_TRADE_TERMS_H

#include "AdvaiModel.h"

namespace Advai
{
struct TradeContents
{
    std::uint32_t Copper = 0;
    std::array<std::pair<std::uint32_t, std::uint32_t>, 6> Items{};
};

inline bool MatchesTrade(TradeOffer const& offer, TradeContents const& own, TradeContents const& peer)
{
    auto const& seller = offer.Selling ? own : peer;
    auto const& buyer = offer.Selling ? peer : own;
    if (!offer.Agreed || !offer.Quantity || seller.Copper || buyer.Copper != offer.Copper)
        return false;
    std::uint64_t count = 0;
    for (auto const& [item, quantity] : seller.Items)
    {
        if (item && item != offer.Item)
            return false;
        if (item == offer.Item)
            count += quantity;
    }
    for (auto const& [item, quantity] : buyer.Items)
        if (item || quantity)
            return false;
    return count == offer.Quantity;
}
}  // namespace Advai

#endif
