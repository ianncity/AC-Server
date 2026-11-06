/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <algorithm>
#include <charconv>
#include <ctime>
#include <stdexcept>

#include "AdvaiDialogue.h"
#include "AdvaiConsent.h"
#include "AdvaiEconomy.h"
#include "AdvaiMgr.h"
#include "AdvaiMoney.h"
#include "ChatHelper.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"
#include "PlayerbotWorldThreadProcessor.h"
#include "Random.h"
#include "World.h"

namespace Advai
{
bool Manager::CanOfferGift(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return !state->Offer && static_cast<Timestamp>(time(nullptr)) >= state->NextGift;
    }
    return false;
}

bool Manager::PrepareGift(BotId giver, BotId receiver, std::uint32_t item, std::uint32_t quantity, bool managedReceiver)
{
    auto from = Get(giver);
    auto to = managedReceiver ? Get(receiver) : nullptr;
    if (!from || giver == receiver || !item || !quantity || (managedReceiver && !to))
        return false;
    auto prepare = [&]
    {
        auto now = static_cast<Timestamp>(time(nullptr));
        if (from->Offer || (to && to->Offer) || now < from->NextGift)
            return false;
        from->NextGift = now + 30;
        from->Offer = TradeOffer{receiver, item, quantity, 0, now + 90, true, true};
        from->RetainActivityBefore(Goal::Trade);
        from->LifeGoal = {Goal::Trade, now, now + 90,         0,
                          receiver,    0,   Outcome::Started, "offering supplies to a group member"};
        from->NextDecision = 0;
        from->Dirty = true;
        if (to)
        {
            to->Offer = TradeOffer{giver, item, quantity, 0, now + 90, false, true};
            to->RetainActivityBefore(Goal::Trade);
            to->LifeGoal = {Goal::Trade, now, now + 90,         0,
                            giver,       0,   Outcome::Started, "receiving offered group supplies"};
            to->NextDecision = 0;
            to->Dirty = true;
        }
        return true;
    };
    if (to)
    {
        std::scoped_lock lock(from->Mutex, to->Mutex);
        return prepare();
    }
    std::lock_guard lock(from->Mutex);
    return prepare();
}

std::optional<TradeOffer> Manager::Offer(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        return state->Offer;
    }
    return std::nullopt;
}

bool Manager::ItemReserved(BotId id, std::uint32_t item) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (std::find(state->CraftReservations.begin(), state->CraftReservations.end(), item) !=
            state->CraftReservations.end())
            return true;
        auto const& offer = state->Offer;
        return offer && offer->Selling && offer->Item == item &&
               offer->Expires > static_cast<Timestamp>(time(nullptr));
    }
    return false;
}

void Manager::ReserveCraft(BotId id, std::uint32_t recipe, std::vector<std::uint32_t> items)
{
    std::sort(items.begin(), items.end());
    items.erase(std::unique(items.begin(), items.end()), items.end());
    if (items.size() > 128)
        throw std::runtime_error("ADVAI craft reservation exceeds bounded recipe depth");
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (state->LifeGoal.Kind != Goal::Craft)
            return;
        if (state->CraftReservations != items)
        {
            state->CraftReservations = std::move(items);
            state->Dirty = true;
        }
        if (state->LifeGoal.Target != recipe)
        {
            state->LifeGoal.Target = recipe;
            state->Dirty = true;
        }
    }
}

std::uint32_t Manager::CraftRecipe(BotId id) const
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (state->LifeGoal.Kind == Goal::Craft)
            return state->LifeGoal.Target;
        for (auto it = state->InterruptedGoals.rbegin(); it != state->InterruptedGoals.rend(); ++it)
            if (it->Kind == Goal::Craft)
                return it->Target;
    }
    return 0;
}

void Manager::MailDispatched(BotId id, TradeOffer const& expected)
{
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        auto const& offer = state->Offer;
        if (!offer || !offer->ByMail || offer->Peer != expected.Peer || offer->Item != expected.Item ||
            offer->Quantity != expected.Quantity || offer->Copper != expected.Copper)
            throw std::runtime_error("ADVAI COD dispatch lost its negotiated terms");
        std::erase_if(state->Messages, [&](auto const& message)
            { return message.Topic == Intent::TradeOffer && message.Sender == expected.Peer; });
        state->Offer.reset();
        GoalPlanner::ApplyResult(state->LifeGoal,
            {Goal::Trade, Outcome::Succeeded, "COD attachment dispatched; payment remains pending in native mail"},
            static_cast<Timestamp>(time(nullptr)));
        // Dispatch does not establish payment or a completed sale. Native mail owns the attachment and debt.
        state->NextSnapshot = 0;
        state->Dirty = true;
    }
}

void Manager::FinishTrade(BotId id, bool completed, std::string const& reason, TradeOffer const* expected)
{
    std::optional<TradeOffer> reciprocal;
    if (auto state = Get(id))
    {
        std::lock_guard lock(state->Mutex);
        if (!state->Offer)
            return;
        if (expected && (state->Offer->Peer != expected->Peer || state->Offer->Item != expected->Item ||
                         state->Offer->Quantity != expected->Quantity || state->Offer->Copper != expected->Copper ||
                         state->Offer->Selling != expected->Selling || state->Offer->Agreed != expected->Agreed))
            return;
        if (state->Offer->Agreed)
            reciprocal = state->Offer;
        if (!state->Offer->Copper && state->Offer->Selling)
            state->NextGift = static_cast<Timestamp>(time(nullptr)) + (completed ? 15 : 600);
        std::erase_if(state->Messages, [&](auto const& message)
                      { return message.Topic == Intent::TradeOffer && message.Sender == state->Offer->Peer; });
        if (!completed &&
            !PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<CancelTradeOperation>(ObjectGuid::Create<HighGuid::Player>(id), state->Offer->Peer)))
            LOG_ERROR("playerbots", "ADVAI could not queue failed trade cancellation for {}", id);
        if (completed)
            state->ObserveExperience({state->Offer->Copper ? SocialEventKind::Trade : SocialEventKind::Assistance,
                                      state->Offer->Peer, static_cast<Timestamp>(time(nullptr)), true});
        if (!completed && state->Offer->Agreed)
        {
            ++_failedTradeCommitments;
            state->ObserveExperience(
                {SocialEventKind::BrokenCommitment, 0, static_cast<Timestamp>(time(nullptr)), false});
        }
        if (!completed && state->Messages.size() < 2)
        {
            PendingMessage cancelled;
            cancelled.Sender = state->Offer->Peer;
            cancelled.Channel = CHAT_MSG_WHISPER;
            cancelled.Due = static_cast<Timestamp>(time(nullptr)) + 1;
            cancelled.Expires = cancelled.Due + 90;
            cancelled.Topic = Intent::TradeCancel;
            cancelled.Thread = (static_cast<std::uint64_t>(id) << 32) ^ cancelled.Due;
            cancelled.Locale = TEXT_LOCALE;
            auto speech = SelectCatalogSpeech(CurrentCatalog(), "advai_trade_cancel", cancelled.Locale, rand32(),
                                              state->Conversation.RecentVariants, state->Persona, state->Cognition.Mood,
                                              {false, state->Relationships.count(state->Offer->Peer) != 0, 0});
            cancelled.Text = std::move(speech.Text);
            cancelled.Source = std::move(speech.Source);
            cancelled.Catalog = std::move(speech.Catalog);
            cancelled.Variant = speech.Variant;
            if (!cancelled.Text.empty())
                state->Messages.push_back(std::move(cancelled));
        }
        state->Offer.reset();
        if (state->LifeGoal.Kind == Goal::Trade)
            GoalPlanner::ApplyResult(state->LifeGoal,
                                     {Goal::Trade, completed ? Outcome::Succeeded : Outcome::Cancelled, reason},
                                     static_cast<Timestamp>(time(nullptr)));
        state->NextSnapshot = 0;
        state->Dirty = true;
    }
    if (reciprocal)
    {
        auto peer = reciprocal->Peer;
        reciprocal->Peer = id;
        reciprocal->Selling = !reciprocal->Selling;
        FinishTrade(peer, completed, reason, &*reciprocal);
    }
}

void Manager::TradeStatus(PlayerbotAI* ai, std::uint32_t status)
{
    if (!Applies(ai->GetBot()))
        return;
    if (status == TRADE_STATUS_TRADE_COMPLETE)
        FinishTrade(ai->GetBot()->GetGUID().GetCounter(), true, "trade completion confirmed by server");
    else if (status == TRADE_STATUS_TRADE_CANCELED)
        FinishTrade(ai->GetBot()->GetGUID().GetCounter(), false, "trade cancelled by server or participant");
}

void Manager::Negotiate(PlayerbotAI* ai, Player* sender, ParsedMessage const& message, Intent& intent, BotState& state,
                        Timestamp now, std::uint32_t usage, bool direct)
{
    Player* bot = ai->GetBot();
    BotId other = sender->GetGUID().GetCounter();
    auto isAgreement = [&]
    {
        return AcceptsInvitation(message.Normalized) ||
               message.Normalized == "deal" || message.Normalized == "agreed" || message.Normalized == "sounds good" ||
               (state.Offer && state.Offer->Selling &&
                std::find(message.Tokens.begin(), message.Tokens.end(), "mail") != message.Tokens.end() &&
                std::find(message.Tokens.begin(), message.Tokens.end(), "cod") != message.Tokens.end());
    };
    bool decline = DeclinesInvitation(message.Normalized) || message.Normalized == "forget it" ||
                   message.Normalized == "call it off" || message.Normalized == "changed my mind" ||
                   std::find(message.Tokens.begin(), message.Tokens.end(), "cancel") != message.Tokens.end() ||
                   (message.Negated && (intent == Intent::Buy || intent == Intent::Sell || intent == Intent::Trade));
    if (state.Offer && state.Offer->Peer == other && direct && decline)
    {
        std::erase_if(state.Messages, [other](auto const& pending)
                      { return pending.Topic == Intent::TradeOffer && pending.Sender == other; });
        if (!PlayerbotWorldThreadProcessor::instance().QueueOperation(
                std::make_unique<CancelTradeOperation>(bot->GetGUID(), other)))
            LOG_ERROR("playerbots", "ADVAI could not queue cancellation for {}", bot->GetName());
        state.Offer.reset();
        if (state.LifeGoal.Kind == Goal::Trade)
            GoalPlanner::ApplyResult(state.LifeGoal, {Goal::Trade, Outcome::Cancelled, "participant declined offer"},
                                     now);
        state.Dirty = true;
        intent = Intent::TradeCancel;
        return;
    }
    if (state.Offer && state.Offer->Peer == other && state.Offer->Expires > now && direct && isAgreement() &&
        !message.Negated)
    {
        state.Offer->Agreed = true;
        if (state.Offer->Selling && state.Offer->Copper &&
            std::find(message.Tokens.begin(), message.Tokens.end(), "mail") != message.Tokens.end() &&
            std::find(message.Tokens.begin(), message.Tokens.end(), "cod") != message.Tokens.end())
            state.Offer->ByMail = true;
        state.Offer->Expires = now + 600;
        state.RetainActivityBefore(Goal::Trade);
        state.LifeGoal = {Goal::Trade, now, now + 600, 0, other, 0, Outcome::Started, "meeting for agreed trade"};
        state.Dirty = true;
        intent = Intent::TradeOffer;
    }
    else if (!message.Negated && !message.Items.empty() && (intent == Intent::Buy || intent == Intent::Sell))
    {
        if (bot->GetTradeData() || (state.Offer && state.Offer->Agreed))
        {
            // A replacement quote must wait until the old trade has actually closed.
            intent = Intent::Activity;
            return;
        }
        bool nearbyGroup = bot->GetGroup() && bot->GetGroup() == sender->GetGroup() &&
                           bot->GetMap() == sender->GetMap() && bot->IsWithinDistInMap(sender, 20.0f);
        if ((state.Snapshot.HasCommitment() && !nearbyGroup) || bot->IsInCombat() ||
            (state.Offer && state.Offer->Peer != other && state.Offer->Expires > now))
            return;
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(message.Items.front());
        if (!proto || proto->Bonding == BIND_WHEN_PICKED_UP || proto->Class == ITEM_CLASS_QUEST)
            return;
        bool selling = intent == Intent::Buy;
        std::uint32_t count = 1;
        auto quote = ParsePrice(message.QuantityTokens);
        if (!quote.Valid || (quote.Found && !quote.Copper))
        {
            intent = Intent::Clarify;
            return;
        }
        std::uint32_t copper = quote.Copper;
        bool unitPrice = std::find(message.Tokens.begin(), message.Tokens.end(), "each") != message.Tokens.end() ||
                         std::find(message.Tokens.begin(), message.Tokens.end(), "apiece") != message.Tokens.end() ||
                         std::find(message.Tokens.begin(), message.Tokens.end(), "ea") != message.Tokens.end() ||
                         message.Normalized.find("per item") != std::string::npos;
        if (selling)
        {
            if (std::find(state.CraftReservations.begin(), state.CraftReservations.end(), proto->ItemId) !=
                state.CraftReservations.end())
                return;
            Item* stock = nullptr;
            for (Item* item : ai->GetInventoryAndEquippedItems())
                if (item && item->GetEntry() == proto->ItemId && !item->IsEquipped() && item->CanBeTraded() &&
                    !item->IsInTrade())
                {
                    stock = item;
                    break;
                }
            if (!stock)
                return;
            bool spareConsumable = proto->Class == ITEM_CLASS_CONSUMABLE &&
                bot->GetItemCount(proto->ItemId, false) >= stock->GetCount() + 10;
            if (usage != ITEM_USAGE_AH && usage != ITEM_USAGE_VENDOR && !spareConsumable &&
                !(state.Offer && state.Offer->Item == proto->ItemId))
                return;
            count = stock->GetCount();
            if (unitPrice && copper)
            {
                if (copper > MAX_MONEY_AMOUNT / count)
                {
                    intent = Intent::Clarify;
                    return;
                }
                copper *= count;
            }
            std::uint64_t floor = static_cast<std::uint64_t>(std::max<std::uint32_t>(proto->SellPrice, 1)) * count;
            if (!copper)
            {
                if (!proto->SellPrice)
                    return;
                copper = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    MAX_MONEY_AMOUNT, floor * (200 + state.Persona.Frugality - state.Persona.Generosity / 2) / 100));
            }
            if (copper < floor)
                return;
        }
        else
        {
            if (usage != ITEM_USAGE_EQUIP && usage != ITEM_USAGE_REPLACE && usage != ITEM_USAGE_SKILL &&
                usage != ITEM_USAGE_USE && usage != ITEM_USAGE_AMMO)
                return;
            for (auto const& token : message.QuantityTokens)
            {
                std::uint32_t parsed = 0;
                auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
                if (result.ec == std::errc{} && result.ptr == token.data() + token.size() && parsed && parsed <= 1000)
                {
                    count = parsed;
                    break;
                }
            }
            if (unitPrice && copper)
            {
                if (copper > MAX_MONEY_AMOUNT / count)
                {
                    intent = Intent::Clarify;
                    return;
                }
                copper *= count;
            }
            if (!copper)
            {
                if (!proto->SellPrice)
                    return;
                copper = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                    MAX_MONEY_AMOUNT,
                    static_cast<std::uint64_t>(std::max<std::uint32_t>(proto->SellPrice, 1)) * count * 2));
            }
            if (!copper || copper > bot->GetMoney() * (100ULL - state.Persona.Frugality / 2) / 100)
                return;
        }
        bool matched = state.Offer && state.Offer->Peer == other && state.Offer->Item == proto->ItemId &&
                       state.Offer->Quantity == count && state.Offer->Copper == copper;
        bool agreed = Applies(sender) && copper && (matched || !selling);
        std::erase_if(state.Messages, [other](auto const& pending)
                      { return pending.Topic == Intent::TradeOffer && pending.Sender == other; });
        state.Offer = TradeOffer{other, proto->ItemId, count, copper, now + (agreed ? 600 : 120), selling, agreed};
        if (agreed)
        {
            state.RetainActivityBefore(Goal::Trade);
            state.LifeGoal = {Goal::Trade, now, now + 600, 0, other, 0, Outcome::Started, "meeting for agreed trade"};
        }
        state.Dirty = true;
        intent = Intent::TradeOffer;
    }
}
}  // namespace Advai
