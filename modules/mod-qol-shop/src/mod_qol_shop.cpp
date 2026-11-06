#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "Item.h"
#include "ItemScript.h"
#include "DatabaseEnv.h"
#include "Config.h"
#include "Log.h"
#include "../../AscensionHookGuard.h"
#include "ObjectMgr.h"
#include "GameTime.h"
#include "WorldSession.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <string>

using namespace Acore::ChatCommands;

namespace
{
constexpr uint32 ITEM_TOME_OF_ADVANCEMENT = 98500;
constexpr uint32 ITEM_TOME_OF_CLEAN_SLATE = 98501;
constexpr uint32 ITEM_DRAUGHT_OF_ACCELERATED_LEARNING = 98502;
constexpr uint32 ITEM_PORTABLE_BANK_VAULT = 98505;
std::unordered_map<uint32, uint64> ActiveXPDraughts;
std::mutex ActiveXPDraughtsMutex;

bool IsQoLShopEnabled()
{
    static bool const enabled = sConfigMgr->GetOption<bool>("QoLShop.Enable", true);
    return enabled;
}

uint32 GetQoLShopXPDraughtDuration()
{
    static uint32 const duration = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("QoLShop.XPDraughtDurationSeconds", 3600));
    return duration;
}

void StartXPDraught(Player* player)
{
    uint32 guid = player->GetGUID().GetCounter();
    uint64 now = uint64(GameTime::GetGameTime().count());
    uint64 previousExpiry = 0;
    if (QueryResult result = CharacterDatabase.Query("SELECT xpdraught_expires FROM character_dp WHERE guid = {}", guid))
        previousExpiry = (*result)[0].Get<uint64>();

    uint64 expiry = std::max(now, previousExpiry) + GetQoLShopXPDraughtDuration();
    CharacterDatabase.DirectExecute("INSERT INTO character_dp (guid, xpdraught_expires) VALUES ({}, {}) ON DUPLICATE KEY UPDATE xpdraught_expires = {}", guid, expiry, expiry);
    {
        std::lock_guard<std::mutex> lock(ActiveXPDraughtsMutex);
        ActiveXPDraughts[guid] = expiry;
    }
    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff[Playtime Shop]|r Your experience draught is active for {} more minute(s). Reusing a draught extends its duration.", (expiry - now + 59) / 60);
}
}

static uint32 GetQoLShopMinutesPerDP()
{
    static uint32 const minutes = std::max<uint32>(1, sConfigMgr->GetOption<uint32>("QoLShop.MinutesPerDP", 10));
    return minutes;
}

static uint32 GetQoLShopAvailableDP(Player* player)
{
    uint32 earnedDP = (player->GetTotalPlayedTime() / 60) / GetQoLShopMinutesPerDP();
    QueryResult result = CharacterDatabase.Query("SELECT spent_dp FROM character_dp WHERE guid = {}", player->GetGUID().GetCounter());
    uint32 spentDP = result ? (*result)[0].Get<uint32>() : 0;
    return earnedDP > spentDP ? earnedDP - spentDP : 0;
}

class mod_qol_shop_playerScript : public PlayerScript
{
public:
    mod_qol_shop_playerScript() : PlayerScript("mod_qol_shop_playerScript") {}

    void OnPlayerLogin(Player* player) override
    {
        ASCENSION_GUARD("qol_shop::OnPlayerLogin", [&] { LoginImpl(player); });
    }

    void LoginImpl(Player* player)
    {
        if (!player || !IsQoLShopEnabled())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        if (QueryResult result = CharacterDatabase.Query("SELECT xpdraught_expires FROM character_dp WHERE guid = {}", guid))
        {
            uint64 expiry = (*result)[0].Get<uint64>();
            if (expiry > uint64(GameTime::GetGameTime().count()))
            {
                std::lock_guard<std::mutex> lock(ActiveXPDraughtsMutex);
                ActiveXPDraughts[guid] = expiry;
            }
            else
            {
                {
                    std::lock_guard<std::mutex> lock(ActiveXPDraughtsMutex);
                    ActiveXPDraughts.erase(guid);
                }
                if (expiry)
                    CharacterDatabase.Execute("UPDATE character_dp SET xpdraught_expires = 0 WHERE guid = {} AND xpdraught_expires = {}", guid, expiry);
            }
        }

        uint32 totalPlayedMinutes = player->GetTotalPlayedTime() / 60;
        uint32 minutesPerDP = GetQoLShopMinutesPerDP();
        uint32 availableDP = GetQoLShopAvailableDP(player);
        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff[Playtime Shop]|r You earn 1 DP every {} minutes played. Total playtime: {} minutes; available balance: {} DP. Type |cff00ff00/shop|r to open the shop.", minutesPerDP, totalPlayedMinutes, availableDP);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
        {
            std::lock_guard<std::mutex> lock(ActiveXPDraughtsMutex);
            ActiveXPDraughts.erase(player->GetGUID().GetCounter());
        }
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (!player || !amount || !IsQoLShopEnabled())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint64 now = uint64(GameTime::GetGameTime().count());
        uint64 expiry = 0;
        {
            std::lock_guard<std::mutex> lock(ActiveXPDraughtsMutex);
            auto itr = ActiveXPDraughts.find(guid);
            if (itr == ActiveXPDraughts.end())
                return;
            expiry = itr->second;
            if (expiry <= now)
                ActiveXPDraughts.erase(itr);
        }

        if (expiry <= now)
        {
            CharacterDatabase.Execute("UPDATE character_dp SET xpdraught_expires = 0 WHERE guid = {} AND xpdraught_expires <= {}", guid, now);
            ChatHandler(player->GetSession()).SendSysMessage("|cff00ffff[Playtime Shop]|r Your experience draught has expired.");
            return;
        }

        amount = uint32(std::min<uint64>(uint64(amount) * 2, std::numeric_limits<uint32>::max()));
    }
};

class item_qol_shop : public ItemScript
{
public:
    item_qol_shop() : ItemScript("item_qol_shop") {}

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!player || !item)
            return false;

        if (!IsQoLShopEnabled())
        {
            ChatHandler(player->GetSession()).SendSysMessage("The playtime rewards shop is currently disabled.");
            player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
            return true;
        }

        switch (item->GetEntry())
        {
            case ITEM_TOME_OF_ADVANCEMENT:
                ChatHandler(player->GetSession()).SendSysMessage("QOLSHOP_OPEN_GRIMOIRE");
                break;
            case ITEM_TOME_OF_CLEAN_SLATE:
                if (!player->resetTalents(true))
                {
                    ChatHandler(player->GetSession()).SendSysMessage("You have no spent talent points to reset. Your tome was not consumed.");
                    player->SendEquipError(EQUIP_ERR_NONE, item, nullptr);
                    return true;
                }
                player->SendTalentsInfoData(false);
                ChatHandler(player->GetSession()).SendSysMessage("|cff00ffff[Playtime Shop]|r Your talents have been reset.");
                break;
            case ITEM_DRAUGHT_OF_ACCELERATED_LEARNING:
                StartXPDraught(player);
                break;
            case ITEM_PORTABLE_BANK_VAULT:
                // Reusable: open the bank and keep the item.
                player->GetSession()->SendShowBank(player->GetGUID());
                return true;
            default:
                return false;
        }

        player->DestroyItemCount(item->GetEntry(), 1, true);
        return true;
    }
};

class qol_shop_commandscript : public CommandScript
{
public:
    qol_shop_commandscript() : CommandScript("qol_shop_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable shopCommandTable =
        {
            { "buy",     HandleShopBuy,     SEC_PLAYER, Console::No },
            { "balance", HandleShopBalance, SEC_PLAYER, Console::No },
            { "",        HandleShopHelp,    SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "shop", shopCommandTable },
            { "dp",   shopCommandTable }
        };

        return commandTable;
    }

    static uint32 GetSpentDP(uint32 guid)
    {
        QueryResult result = CharacterDatabase.Query("SELECT spent_dp FROM character_dp WHERE guid = {}", guid);
        if (result)
        {
            Field* fields = result->Fetch();
            return fields[0].Get<uint32>();
        }
        return 0;
    }

    static void AddSpentDP(uint32 guid, uint32 cost)
    {
        // Complete the debit before the next balance query. Execute() queues this
        // write asynchronously, so rapid purchases could otherwise reuse a stale
        // spent_dp value and spend the same playtime points more than once.
        CharacterDatabase.DirectExecute("INSERT INTO character_dp (guid, spent_dp) VALUES ({}, {}) ON DUPLICATE KEY UPDATE spent_dp = spent_dp + {}", guid, cost, cost);
    }

    static uint32 GetAvailableDP(Player* player)
    {
        uint32 totalMinutes = player->GetTotalPlayedTime() / 60;
        uint32 earnedDP = totalMinutes / GetQoLShopMinutesPerDP();
        uint32 spent = GetSpentDP(player->GetGUID().GetCounter());
        return (earnedDP > spent) ? (earnedDP - spent) : 0;
    }

    static bool HandleShopHelp(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!IsQoLShopEnabled())
        {
            handler->SendSysMessage("The playtime rewards shop is currently disabled.");
            return true;
        }

        uint32 available = GetAvailableDP(player);
        uint32 totalMinutes = player->GetTotalPlayedTime() / 60;

        handler->PSendSysMessage("|cff00ffff=== PLAYTIME REWARDS ===|r");
        handler->PSendSysMessage("Rate: {} Minutes Played = 1 DP | Total Played: {} mins ({} hrs {} mins)", GetQoLShopMinutesPerDP(), totalMinutes, totalMinutes / 60, totalMinutes % 60);
        handler->PSendSysMessage("Available Balance: |cff00ff00{} DP|r", available);
        handler->SendSysMessage("---------------------------------------------");
        handler->SendSysMessage("|cff00ff00.shop buy book|r   - Tome of Advancement (60 DP - Opens your class grimoire)");
        handler->SendSysMessage("|cff00ff00.shop buy bank|r   - Portable Bank Vault (120 DP - reusable, opens your bank anywhere)");
        handler->SendSysMessage("|cff00ff00.shop buy mail|r   - Portable Mailbox Unit (180 DP)");
        handler->SendSysMessage("|cff00ff00.shop buy repair|r - Field Repair Bot & Reagents (120 DP)");
        handler->SendSysMessage("|cff00ff00.shop buy respec|r - Reset Talents Anywhere (60 DP)");
        handler->SendSysMessage("|cff00ff00.shop buy xpdraught|r - Draught of Accelerated Learning (60 DP - doubles earned XP for the configured duration)");
        handler->SendSysMessage("Use |cff00ff00/shop|r to browse rewards, or the commands above to claim them.");
        return true;
    }

    static bool HandleShopBalance(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!IsQoLShopEnabled())
        {
            handler->SendSysMessage("The playtime rewards shop is currently disabled.");
            return true;
        }

        uint32 available = GetAvailableDP(player);
        uint32 totalMinutes = player->GetTotalPlayedTime() / 60;
        handler->PSendSysMessage("|cff00ffff[Playtime Shop]|r Playtime: {} minutes | Available Balance: |cff00ff00{} DP|r", totalMinutes, available);
        return true;
    }

    static bool HandleShopBuy(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player || !*args)
        {
            handler->SendSysMessage("Syntax: .shop buy <book|bank|mail|repair|respec|xpdraught>");
            return false;
        }

        if (!IsQoLShopEnabled())
        {
            handler->SendSysMessage("The playtime rewards shop is currently disabled.");
            return true;
        }

        std::string choice = args;
        uint32 cost = 0;
        uint32 itemToAdd = 0;
        std::string itemName = "";

        if (choice == "book")
        {
            if (player->getClass() != CLASS_CULTIST && player->getClass() != CLASS_NECROMANCER)
            {
                handler->SendSysMessage("The class advancement grimoire is only available to Cultists and Necromancers.");
                return false;
            }
            cost = 60;
            itemToAdd = 98500; // Tome of Advancement
            itemName = "Tome of Advancement (Portable Upgrade Book)";
        }
        else if (choice == "bank")
        {
            cost = 120;
            itemToAdd = ITEM_PORTABLE_BANK_VAULT;
            itemName = "Portable Bank Vault";
        }
        else if (choice == "mail")
        {
            cost = 180;
            itemToAdd = 98503; // Portable MOLL-E without Engineering requirement
            itemName = "Portable Mailbox (MOLL-E)";
        }
        else if (choice == "repair")
        {
            cost = 120;
            itemToAdd = 98504; // Field Repair Bot without Engineering requirement
            itemName = "Portable Field Repair Bot";
        }
        else if (choice == "respec")
        {
            cost = 60;
            itemToAdd = 98501; // Tome of Clean Slate
            itemName = "Tome of Clean Slate (Portable Respec)";
        }
        else if (choice == "xpdraught")
        {
            cost = 60;
            itemToAdd = 98502; // Draught of Accelerated Learning (+100% XP)
            itemName = "Draught of Accelerated Learning (+100% EXP)";
        }
        else
        {
            handler->SendSysMessage("Unknown item. Options: book, bank, mail, repair, respec, xpdraught");
            return false;
        }

        uint32 available = GetAvailableDP(player);
        if (available < cost)
        {
            handler->PSendSysMessage("|cffff2020[Playtime Shop]|r Insufficient DP! Cost: {} DP. You have {} DP ({} more playtime minutes required).", cost, available, (cost - available) * GetQoLShopMinutesPerDP());
            return false;
        }

        if (itemToAdd)
        {
            if (!sObjectMgr->GetItemTemplate(itemToAdd))
            {
                LOG_ERROR("module", "QoL Shop item {} is missing from item_template; refusing purchase.", itemToAdd);
                handler->SendSysMessage("This shop item is unavailable because its item data is missing.");
                return false;
            }

            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemToAdd, 1);
            if (msg != EQUIP_ERR_OK)
            {
                player->SendEquipError(msg, nullptr, nullptr, itemToAdd);
                return false;
            }

            Item* item = player->StoreNewItem(dest, itemToAdd, true);
            if (!item)
            {
                LOG_ERROR("module", "QoL Shop item {} could not be stored for player {}; no DP was charged.",
                    itemToAdd, player->GetName());
                handler->SendSysMessage("The item could not be stored. No DP was charged.");
                return false;
            }

            player->SendNewItem(item, 1, true, false);
            AddSpentDP(player->GetGUID().GetCounter(), cost);
            handler->PSendSysMessage("|cff00ffff[Playtime Shop]|r Purchased {} for {} DP! Added to inventory.", itemName, cost);
        }

        return true;
    }
};

void Addmod_qol_shopScripts()
{
    new mod_qol_shop_playerScript();
    new qol_shop_commandscript();
    new item_qol_shop();
}
