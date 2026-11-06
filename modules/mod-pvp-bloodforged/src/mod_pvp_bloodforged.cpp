#include "ScriptMgr.h"
#include "Player.h"
#include "Battleground.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Random.h"

#include <algorithm>

using namespace Acore::ChatCommands;

enum BloodforgedItems
{
    ITEM_GLADIATOR_SPOILS = 98510
};

static bool IsBloodforgedEnabled()
{
    static bool const enabled = sConfigMgr->GetOption<bool>("PvPBloodforged.Enable", true);
    return enabled;
}

static uint32 GetBloodforgedItemChance()
{
    static uint32 const chance = std::min<uint32>(sConfigMgr->GetOption<uint32>("PvPBloodforged.ItemChance", 20), 100);
    return chance;
}

static uint32 GetBloodforgedRareChance()
{
    static uint32 const chance = std::min<uint32>(sConfigMgr->GetOption<uint32>("PvPBloodforged.RareChance", 70), 100);
    return chance;
}

static uint32 GetBloodforgedEpicChance()
{
    static uint32 const chance = std::min<uint32>(sConfigMgr->GetOption<uint32>("PvPBloodforged.EpicChance", 30), 100);
    return chance;
}

struct BracketLoot
{
    uint32 minLevel;
    uint32 maxLevel;
    uint32 rareEntry;
    uint32 epicEntry;
    uint32 minCopper;
    uint32 maxCopper;
};

static const BracketLoot g_Brackets[] = {
    { 10, 19, 96210, 96310,  5000,   10000 }, // 50s - 1g
    { 20, 29, 96220, 96320, 10000,   25000 }, // 1g - 2.5g
    { 30, 39, 96230, 96330, 30000,   60000 }, // 3g - 6g
    { 40, 49, 96240, 96340, 70000,  120000 }, // 7g - 12g
    { 50, 59, 96250, 96350, 150000, 220000 }, // 15g - 22g
    { 60, 69, 96260, 96360, 250000, 400000 }, // 25g - 40g
    { 70, 79, 96270, 96370, 450000, 650000 }, // 45g - 65g
    { 80, 80, 96280, 96380, 750000,1250000 }  // 75g - 125g
};

// Per-character daily cap on gold paid out by opened spoils (PvPBloodforged.DailyGoldCapGold, in gold; 0 = no cap).
// Totals are kept per character per server day (GameTime / 86400) in character_bloodforged_gold.
static uint64 GetBloodforgedDailyGoldCapCopper()
{
    static uint64 const capCopper = uint64(sConfigMgr->GetOption<uint32>("PvPBloodforged.DailyGoldCapGold", 500)) * 10000;
    return capCopper;
}

static uint64 GetBloodforgedGoldEarnedToday(uint32 guid, uint32 day)
{
    QueryResult result = CharacterDatabase.Query("SELECT copper FROM character_bloodforged_gold WHERE guid = {} AND day = {}", guid, day);
    return result ? (*result)[0].Get<uint64>() : 0;
}

static void RecordBloodforgedGold(uint32 guid, uint32 day, uint32 money)
{
    CharacterDatabase.DirectExecute("INSERT INTO character_bloodforged_gold (guid, day, copper) VALUES ({}, {}, {}) ON DUPLICATE KEY UPDATE copper = copper + {}", guid, day, uint64(money), uint64(money));
}

class item_gladiator_spoils : public ItemScript
{
public:
    item_gladiator_spoils() : ItemScript("item_gladiator_spoils") {}

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!player || !item)
            return false;

        if (!IsBloodforgedEnabled())
        {
            ChatHandler(player->GetSession()).SendSysMessage("PvP Bloodforged rewards are currently disabled. Your spoils remain unopened.");
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true;
        }

        uint32 level = player->GetLevel();
        if (level < 10)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffff2020You must be at least level 10 to open Gladiator's Spoils.|r");
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true;
        }

        BracketLoot const* currentBracket = nullptr;
        for (const auto& b : g_Brackets)
        {
            if (level >= b.minLevel && level <= b.maxLevel)
            {
                currentBracket = &b;
                break;
            }
        }

        if (!currentBracket)
        {
            LOG_ERROR("module", "Bloodforged has no reward bracket for player {} at level {}.", player->GetName(), level);
            ChatHandler(player->GetSession()).SendSysMessage("No Bloodforged reward bracket is configured for your level. Your spoils remain unopened.");
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true;
        }

        uint32 itemChance = GetBloodforgedItemChance();
        uint32 roll = urand(1, 100);
        if (roll <= itemChance)
        {
            uint32 rareWeight = GetBloodforgedRareChance();
            uint32 epicWeight = GetBloodforgedEpicChance();
            uint32 totalWeight = rareWeight + epicWeight;
            if (!totalWeight)
            {
                LOG_ERROR("module", "Bloodforged rare and epic reward weights are both zero; preserving the unopened spoils.");
                ChatHandler(player->GetSession()).SendSysMessage("Bloodforged item rewards are unavailable because their reward weights are invalid. Your spoils remain unopened.");
                player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
                return true;
            }

            bool isRare = urand(1, totalWeight) <= rareWeight;
            uint32 awardedItem = isRare ? currentBracket->rareEntry : currentBracket->epicEntry;

            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, awardedItem, 1);
            if (msg != EQUIP_ERR_OK)
            {
                player->SendEquipError(msg, nullptr, nullptr, awardedItem);
                ChatHandler(player->GetSession()).SendSysMessage("Your inventory could not hold the weapon. The spoils remain unopened.");
                return true;
            }

            Item* newItem = player->StoreNewItem(dest, awardedItem, true);
            if (!newItem)
            {
                LOG_ERROR("module", "Bloodforged reward item {} failed to store for player {} after inventory preflight.",
                          awardedItem, player->GetName());
                ChatHandler(player->GetSession()).SendSysMessage("The weapon could not be stored. The spoils remain unopened.");
                player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
                return true;
            }

            player->DestroyItemCount(item->GetEntry(), 1, true);
            player->SendNewItem(newItem, 1, true, false);

            if (isRare)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cff0070dd[Bloodforged]|r The bag bursts open! You discovered a Rare Bloodforged weapon for your level bracket!");
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("|cffa335ee[Bloodforged]|r An Epic Bloodforged artifact shines brightly within the spoils!");
            }
        }
        else
        {
            // 80% chance: Scaled Silver / Gold
            uint32 money = urand(currentBracket->minCopper, currentBracket->maxCopper);
            uint32 const guid = player->GetGUID().GetCounter();
            uint32 const day = uint32(GameTime::GetGameTime().count() / 86400);
            uint64 const dailyCap = GetBloodforgedDailyGoldCapCopper();
            if (dailyCap)
            {
                uint64 const earned = GetBloodforgedGoldEarnedToday(guid, day);
                if (earned >= dailyCap)
                {
                    ChatHandler(player->GetSession()).SendSysMessage("You have reached today's Bloodforged gold limit. Your spoils remain unopened until the daily reset.");
                    player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
                    return true;
                }
                if (money > dailyCap - earned)
                    money = static_cast<uint32>(dailyCap - earned);
            }

            if (!player->ModifyMoney(money))
            {
                ChatHandler(player->GetSession()).SendSysMessage("Your coin purse cannot hold this reward. The spoils remain unopened.");
                player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
                return true;
            }

            if (dailyCap)
                RecordBloodforgedGold(guid, day, money);

            player->DestroyItemCount(item->GetEntry(), 1, true);

            uint32 gold = money / 10000;
            uint32 silver = (money % 10000) / 100;
            ChatHandler(player->GetSession()).PSendSysMessage("|cffffff00[Gladiator's Spoils]|r You opened the bag and found {} Gold, {} Silver!", gold, silver);
        }

        return true;
    }
};

class mod_pvp_battleground_reward : public BGScript
{
public:
    mod_pvp_battleground_reward() : BGScript("mod_pvp_battleground_reward") {}

    void OnBattlegroundEnd(Battleground* bg, TeamId /*winnerTeam*/) override
    {
        if (!bg || bg->isArena() || !IsBloodforgedEnabled())
            return;

        if (!sObjectMgr->GetItemTemplate(ITEM_GLADIATOR_SPOILS))
        {
            LOG_ERROR("module", "Bloodforged reward item {} is missing; no battleground rewards can be granted.",
                ITEM_GLADIATOR_SPOILS);
            return;
        }

        for (auto const& pair : bg->GetPlayers())
        {
            Player* player = ObjectAccessor::FindPlayer(pair.first);
            if (player && player->IsInWorld())
            {
                ItemPosCountVec dest;
                InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_GLADIATOR_SPOILS, 1);
                if (msg != EQUIP_ERR_OK && msg != EQUIP_ERR_INVENTORY_FULL)
                {
                    player->SendEquipError(msg, nullptr, nullptr, ITEM_GLADIATOR_SPOILS);
                    ChatHandler(player->GetSession()).SendSysMessage("|cffff2020[PvP Reward]|r You cannot receive another Gladiator's Spoils right now.");
                    continue;
                }
                Item* bag = msg == EQUIP_ERR_OK ? player->StoreNewItem(dest, ITEM_GLADIATOR_SPOILS, true) : nullptr;
                if (bag)
                {
                    player->SendNewItem(bag, 1, true, false);
                    ChatHandler(player->GetSession()).SendSysMessage("|cff00ffff[PvP Reward]|r You have received a [Gladiator's Spoils of War] for completing the battleground!");
                }
                else
                {
                    player->SendItemRetrievalMail({ { ITEM_GLADIATOR_SPOILS, 1 } });
                    if (msg == EQUIP_ERR_OK)
                    {
                        LOG_ERROR("module", "Gladiator's Spoils could not be stored for player {} after inventory preflight; sent by Postmaster mail.",
                                  player->GetName());
                    }
                    else
                    {
                        LOG_INFO("module", "Gladiator's Spoils sent by Postmaster to {} because inventory could not store it.",
                                 player->GetName());
                    }
                    ChatHandler(player->GetSession()).SendSysMessage("|cff00ffff[PvP Reward]|r Your Gladiator's Spoils were sent by mail because your bags could not hold them.");
                }
            }
        }
    }
};

class pvp_bloodforged_commandscript : public CommandScript
{
public:
    pvp_bloodforged_commandscript() : CommandScript("pvp_bloodforged_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable pvpTable =
        {
            { "bag", HandleGetPvPBag, SEC_GAMEMASTER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "bloodforged", pvpTable },
            { "pvpbag", HandleGetPvPBag, SEC_GAMEMASTER, Console::No }
        };

        return commandTable;
    }

    static bool HandleGetPvPBag(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!IsBloodforgedEnabled())
        {
            handler->SendSysMessage("PvP Bloodforged rewards are currently disabled.");
            return true;
        }

        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_GLADIATOR_SPOILS, 1);
        if (msg == EQUIP_ERR_OK)
        {
            Item* bag = player->StoreNewItem(dest, ITEM_GLADIATOR_SPOILS, true);
            if (!bag)
            {
                LOG_ERROR("module", "Gladiator's Spoils could not be stored for player {} after inventory preflight.", player->GetName());
                handler->SendSysMessage("The reward could not be stored. No spoils were created.");
                return true;
            }

            player->SendNewItem(bag, 1, true, false);
            handler->SendSysMessage("|cff00ffff[PvP Bloodforged]|r [Gladiator's Spoils of War] added to your inventory! Right-click to open.");
        }
        else
        {
            player->SendEquipError(msg, nullptr, nullptr, ITEM_GLADIATOR_SPOILS);
        }

        return true;
    }
};

void AddSC_mod_pvp_bloodforged();
void Addmod_pvp_bloodforgedScripts()
{
    new item_gladiator_spoils();
    new mod_pvp_battleground_reward();
    new pvp_bloodforged_commandscript();
}
