/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_ECONOMY_H
#define PLAYERBOTS_ADVAI_ECONOMY_H

#include "AdvaiModel.h"
#include "PlayerbotOperation.h"

namespace Advai
{
constexpr uint32 SERVICE_MAILBOX = 0x80000000U;

class BankOperation : public PlayerbotOperation
{
public:
    BankOperation(ObjectGuid bot, ObjectGuid banker, std::vector<ObjectGuid> items, bool withdraw,
                  Goal goal = Goal::Bank)
        : _bot(bot), _banker(banker), _items(std::move(items)), _withdraw(withdraw), _goal(goal)
    {
    }
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI normal bank transfer"; }

private:
    ObjectGuid _bot;
    ObjectGuid _banker;
    std::vector<ObjectGuid> _items;
    bool _withdraw;
    Goal _goal;
};

class AuctionOperation : public PlayerbotOperation
{
public:
    AuctionOperation(ObjectGuid bot, ObjectGuid auctioneer, ObjectGuid item, uint8 frugality)
        : _bot(bot), _auctioneer(auctioneer), _item(item), _frugality(frugality)
    {
    }
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI auction"; }

private:
    ObjectGuid _bot;
    ObjectGuid _auctioneer;
    ObjectGuid _item;
    uint8 _frugality;
};

class TradeOperation : public PlayerbotOperation
{
public:
    TradeOperation(ObjectGuid bot, TradeOffer offer) : _bot(bot), _offer(offer) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI negotiated trade"; }

private:
    ObjectGuid _bot;
    TradeOffer _offer;
};

class GiftOperation : public PlayerbotOperation
{
public:
    GiftOperation(ObjectGuid bot, ObjectGuid recipient, ObjectGuid item) : _bot(bot), _recipient(recipient), _item(item)
    {
    }
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI offer supplies through trade"; }

private:
    ObjectGuid _bot;
    ObjectGuid _recipient;
    ObjectGuid _item;
};

class SendCodOperation : public PlayerbotOperation
{
public:
    SendCodOperation(ObjectGuid bot, ObjectGuid mailbox, TradeOffer offer)
        : _bot(bot), _mailbox(mailbox), _offer(offer) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI dispatch agreed COD mail"; }

private:
    ObjectGuid _bot;
    ObjectGuid _mailbox;
    TradeOffer _offer;
};

class CollectMailOperation : public PlayerbotOperation
{
public:
    CollectMailOperation(ObjectGuid bot, ObjectGuid mailbox) : _bot(bot), _mailbox(mailbox) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI collect mail"; }

private:
    ObjectGuid _bot;
    ObjectGuid _mailbox;
};

class CancelTradeOperation : public PlayerbotOperation
{
public:
    CancelTradeOperation(ObjectGuid bot, BotId peer) : _bot(bot), _peer(peer) {}
    bool Execute() override;
    ObjectGuid GetBotGuid() const override { return _bot; }
    std::string GetName() const override { return "ADVAI cancel trade"; }

private:
    ObjectGuid _bot;
    BotId _peer;
};
}  // namespace Advai

#endif
