#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Item.h"
#include "Chat.h"
#include "ScriptedGossip.h"
#include "DatabaseEnv.h"
#include "Config.h"
#include "ObjectMgr.h"
#include "WorldSessionMgr.h"

using namespace Acore::ChatCommands;

enum PrestigeItems
{
    ITEM_PRESTIGE_TABARD    = 96010,
    ITEM_PRESTIGE_HELM      = 96011,
    ITEM_PRESTIGE_CLOAK     = 96012,
    ITEM_PRESTIGE_SWORD     = 96013,
    ITEM_PRESTIGE_PARAGON_H = 96014,
    ITEM_PRESTIGE_SCYTHE    = 96015,
    ITEM_PRESTIGE_SHOULDER  = 96016,
    ITEM_PRESTIGE_SHIELD    = 96017,
    ITEM_PRESTIGE_MOUNT     = 96018,
    ITEM_PRESTIGE_VOIDBLADE = 96019,
    ITEM_TOME_FLIGHT_PATHS  = 96020,
    ITEM_PRESTIGE_BAG       = 96021
};

// Curated, usable mount items awarded upon prestige. Keep this list limited to
// item_template entries with a summon/use spell in the active world database.
static const std::vector<uint32> PrestigeEpicMounts = {
    13327, // Fluorescent Green Mechanostrider
    13326, // White Mechanostrider Mod B
    13328, // Black Ram
    13329, // Frost Ram
    13334, // Green Skeletal Warhorse
    12302, // Reins of the Ancient Frostsaber
    12303, // Tiger Riding Kodo
    49282, // Big Battle Bear
    49284, // Reins of the Swift Spectral Tiger
    49283, // Reins of the Spectral Tiger
    45693, // Mimiron's Head
    32458, // Ashes of Al'ar
    44413, // Mekgineer's Chopper
    49286, // X-51 Nether-Rocket X-TREME
    96018  // Reins of the Ascended Celestial Steed
};

// Helper: sets a prestige title only when the DBC entry exists (SetTitle dereferences it).
static void SetPrestigeTitle(Player* player, uint32 titleId)
{
    if (CharTitlesEntry const* title = sCharTitlesStore.LookupEntry(titleId))
        player->SetTitle(title);
    else
        LOG_ERROR("module", "Prestige title {} is missing from CharTitles.dbc; not granted to player {}.", titleId, player->GetName());
}

// Helper: Discovers all flight paths across all continents
static void UnlockAllFlightPaths(Player* player)
{
    for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
    {
        if (sTaxiNodesStore.LookupEntry(i))
            player->m_taxi.SetTaximaskNode(i);
    }
}

static bool GrantPrestigeItem(Player* player, uint32 itemId, std::vector<std::pair<uint32, uint32>>& overflowItems)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
    if (!itemTemplate)
    {
        LOG_ERROR("module", "Prestige reward item {} is missing; cannot grant to player {}.", itemId, player->GetName());
        return false;
    }

    // Several prestige vanity items are intentionally unique and can be
    // awarded again by later ranks. Do not use Postmaster mail to bypass their
    // MaxCount restriction when the player already owns the item.
    if (itemTemplate->MaxCount > 0 && player->HasItemCount(itemId, itemTemplate->MaxCount, true))
        return true;

    ItemPosCountVec destination;
    if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination, itemId, 1) == EQUIP_ERR_OK)
    {
        if (Item* item = player->StoreNewItem(destination, itemId, true))
        {
            player->SendNewItem(item, 1, true, false);
            return true;
        }

        LOG_ERROR("module", "Prestige reward item {} could not be created for player {}; sending it by Postmaster mail.", itemId, player->GetName());
    }

    overflowItems.emplace_back(itemId, 1);
    return false;
}

class item_tome_of_flightpaths : public ItemScript
{
public:
    item_tome_of_flightpaths() : ItemScript("item_tome_of_flightpaths") { }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        if (!player)
            return false;

        UnlockAllFlightPaths(player);
        ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[Prestige]|r All flight paths unlocked.");
        player->DestroyItemCount(ITEM_TOME_FLIGHT_PATHS, 1, true);
        return true;
    }
};

// Helper: Returns the entire full 8-piece class vanity armor set (all 8 pieces at once)
static std::vector<uint32> GetClassVanitySet(uint8 chrClass, uint32 /*rank*/)
{
    uint32 baseId = 96100;
    switch (chrClass)
    {
        case CLASS_WARRIOR:
        case CLASS_DEATH_KNIGHT:
            baseId = 96100; // Might (Plate)
            break;
        case CLASS_PALADIN:
            baseId = 96110; // Valor (Plate)
            break;
        case CLASS_HUNTER:
            baseId = 96120; // Herod's (Mail)
            break;
        case CLASS_ROGUE:
            baseId = 96130; // Shadowcraft (Leather)
            break;
        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_CULTIST:
        case CLASS_NECROMANCER:
            baseId = 96140; // Dreadmist (Cloth)
            break;
        case CLASS_SHAMAN:
            baseId = 96150; // Elements (Mail)
            break;
        case CLASS_DRUID:
            baseId = 96160; // IronFeather (Leather)
            break;
        default:
            baseId = 96100;
            break;
    }

    // Return ALL 8 pieces of the complete set at once!
    return {
        baseId + 1, // Helm
        baseId + 2, // Shoulders
        baseId + 3, // Chest
        baseId + 4, // Gloves
        baseId + 5, // Legs
        baseId + 6, // Boots
        baseId + 7, // Belt
        baseId + 8  // Bracers
    };
}

enum PrestigeGossip
{
    PRESTIGE_GOSSIP_SENDER_MAIN = 100,

    GOSSIP_ACTION_PRESTIGE = 1,
    GOSSIP_ACTION_INFO     = 2
};

static bool IsPrestigeEnabled()
{
    static bool const enabled = sConfigMgr->GetOption<bool>("Prestige.Enable", true);
    return enabled;
}

class npc_prestige_master : public CreatureScript
{
public:
    npc_prestige_master() : CreatureScript("npc_prestige_master") {}

    static uint32 GetPrestigeRank(Player* player)
    {
        QueryResult result = CharacterDatabase.Query("SELECT prestige_rank FROM character_prestige WHERE guid = {}", player->GetGUID().GetCounter());
        if (result)
            return (*result)[0].Get<uint32>();
        return 0;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!IsPrestigeEnabled())
        {
            ChatHandler(player->GetSession()).SendSysMessage("The Prestige system is currently disabled.");
            CloseGossipMenuFor(player);
            return true;
        }

        uint32 rank = GetPrestigeRank(player);
        std::string rankStr = "|cff00ffff[Prestige Master]|r Current Rank: |cff00ff00" + std::to_string(rank) + "|r";
        ChatHandler(player->GetSession()).PSendSysMessage("{}", rankStr);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cffff8000[Prestige]|r Transcend to next Prestige Rank (Resets to Level 1)...", PRESTIGE_GOSSIP_SENDER_MAIN, GOSSIP_ACTION_PRESTIGE);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "How does the Prestige system work?", PRESTIGE_GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO);

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action) override
    {
        ClearGossipMenuFor(player);

        if (!IsPrestigeEnabled())
        {
            ChatHandler(player->GetSession()).SendSysMessage("The Prestige system is currently disabled.");
            CloseGossipMenuFor(player);
            return true;
        }

        if (sender == PRESTIGE_GOSSIP_SENDER_MAIN)
        {
            if (action == GOSSIP_ACTION_INFO)
            {
                CloseGossipMenuFor(player);
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff=== Prestige System Mechanics ===|r");
                ChatHandler(player->GetSession()).PSendSysMessage("1. Reach Level 80 to unlock Prestige.");
                ChatHandler(player->GetSession()).PSendSysMessage("2. Prestiging resets your character to Level 1 with full stat/talent reinitialization.");
                ChatHandler(player->GetSession()).PSendSysMessage("3. Your high-level equipment is safely returned to your inventory.");
                ChatHandler(player->GetSession()).PSendSysMessage("4. Prestige grants rewards automatically when you reach each rank:");
                ChatHandler(player->GetSession()).PSendSysMessage("   - Rank 1: a Tome of Flight Paths, a 28-slot satchel, a random epic mount, a class vanity set, a tabard, and a title.");
                ChatHandler(player->GetSession()).PSendSysMessage("   - Ranks 2 to 4: a 28-slot satchel, a random epic mount, a class vanity set, and a rank reward.");
                ChatHandler(player->GetSession()).PSendSysMessage("   - Rank 5 and above: a random epic mount, paragon vanity items, and the Immortal title.");
                return true;
            }
            else if (action == GOSSIP_ACTION_PRESTIGE)
            {
                CloseGossipMenuFor(player);
                uint32 reqLevel = sConfigMgr->GetOption<uint32>("Prestige.RequiredLevel", 80);
                if (player->GetLevel() < reqLevel)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000You must reach Level {} before you can Prestige!|r", reqLevel);
                    return true;
                }
                if (player->GetTradeData())
                {
                    ChatHandler(player->GetSession()).SendSysMessage("Finish or cancel your trade before prestiging.");
                    return true;
                }

                uint32 currentRank = GetPrestigeRank(player);
                uint32 newRank = currentRank + 1;
                std::vector<uint32> rewardItems;

                if (newRank <= 4)
                {
                    rewardItems.push_back(ITEM_PRESTIGE_BAG);
                    std::vector<uint32> fullSet = GetClassVanitySet(player->getClass(), newRank);
                    rewardItems.insert(rewardItems.end(), fullSet.begin(), fullSet.end());

                    switch (newRank)
                    {
                        case 1:
                            rewardItems.push_back(ITEM_TOME_FLIGHT_PATHS);
                            rewardItems.push_back(ITEM_PRESTIGE_TABARD);
                            break;
                        case 2: rewardItems.push_back(ITEM_PRESTIGE_CLOAK); break;
                        case 3:
                            rewardItems.push_back(player->getClass() == CLASS_WARRIOR || player->getClass() == CLASS_PALADIN ||
                                player->getClass() == CLASS_DEATH_KNIGHT || player->getClass() == CLASS_HUNTER ?
                                ITEM_PRESTIGE_SWORD : ITEM_PRESTIGE_SCYTHE);
                            break;
                        case 4: rewardItems.push_back(ITEM_PRESTIGE_HELM); break;
                    }
                }
                else
                {
                    rewardItems = { ITEM_PRESTIGE_PARAGON_H, ITEM_PRESTIGE_SHOULDER,
                        ITEM_PRESTIGE_VOIDBLADE, ITEM_PRESTIGE_SHIELD };
                }

                std::vector<uint32> eligibleMounts;
                std::vector<uint32> availableMounts;
                for (uint32 mountItem : PrestigeEpicMounts)
                {
                    ItemTemplate const* mountTemplate = sObjectMgr->GetItemTemplate(mountItem);
                    if (!mountTemplate || !mountTemplate->Spells[0].SpellId ||
                        !(mountTemplate->AllowableClass & player->getClassMask()) ||
                        !(mountTemplate->AllowableRace & player->getRaceMask()))
                        continue;

                    eligibleMounts.push_back(mountItem);
                    if (!player->HasItemCount(mountItem, 1, true))
                        availableMounts.push_back(mountItem);
                }

                if (eligibleMounts.empty())
                {
                    ChatHandler(player->GetSession()).SendSysMessage("Prestige cannot proceed because no usable mount rewards are configured for your race and class. No progress was changed.");
                    return true;
                }

                std::vector<uint32> const& mountPool = availableMounts.empty() ? eligibleMounts : availableMounts;
                uint32 chosenMount = mountPool[urand(0, mountPool.size() - 1)];
                rewardItems.push_back(chosenMount);

                for (uint32 itemId : rewardItems)
                {
                    if (!sObjectMgr->GetItemTemplate(itemId))
                    {
                        LOG_ERROR("module", "Prestige rank {} cannot be granted to {}: reward item {} is missing from item_template.", newRank, player->GetName(), itemId);
                        ChatHandler(player->GetSession()).SendSysMessage("Prestige cannot proceed because a configured reward is missing. No progress was changed.");
                        return true;
                    }
                }

                std::vector<Item*> equippedItemsToStore;
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        if (item->GetTemplate()->RequiredLevel > 1)
                        {
                            if (player->CanUnequipItem(item->GetPos(), false) != EQUIP_ERR_OK)
                            {
                                ChatHandler(player->GetSession()).SendSysMessage("Prestige cannot remove your equipped gear right now. No progress was changed.");
                                return true;
                            }
                            equippedItemsToStore.push_back(item);
                        }
                    }
                }

                // Simulate all moves together, including specialized bag restrictions.
                uint32 itemLimitedByLimitCategory = 0;
                if (player->CanStoreItems(equippedItemsToStore.data(), static_cast<int>(equippedItemsToStore.size()),
                    &itemLimitedByLimitCategory) != EQUIP_ERR_OK)
                {
                    ChatHandler(player->GetSession()).SendSysMessage("Prestige cannot store all equipped gear. Free compatible bag space first. No progress was changed.");
                    return true;
                }

                // Safely unequip high-level gear into inventory
                for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    {
                        if (item->GetTemplate()->RequiredLevel > 1)
                        {
                            ItemPosCountVec dest;
                            InventoryResult msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
                            if (msg == EQUIP_ERR_OK)
                            {
                                player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
                                player->StoreItem(dest, item, true);
                            }
                            else
                            {
                                LOG_ERROR("module", "Prestige could not move equipped item {} for player {} after inventory preflight.", item->GetEntry(), player->GetName());
                                ChatHandler(player->GetSession()).SendSysMessage("Prestige could not move all equipped gear. Your level and rank were not changed.");
                                return true;
                            }
                        }
                    }
                }

                // Perform the Prestige level reset
                player->SetLevel(1);
                player->InitStatsForLevel(true);
                player->SetUInt32Value(PLAYER_XP, 0);
                player->SetUInt32Value(PLAYER_NEXT_LEVEL_XP, sObjectMgr->GetXPForLevel(1));
                player->resetTalents(true);
                player->SendTalentsInfoData(false);

                // Update database
                uint32 guid = player->GetGUID().GetCounter();
                CharacterDatabase.Execute("INSERT INTO character_prestige (guid, prestige_rank, total_prestiges) VALUES ({}, 1, 1) ON DUPLICATE KEY UPDATE prestige_rank = {}, total_prestiges = total_prestiges + 1", guid, newRank);
                std::vector<std::pair<uint32, uint32>> overflowRewards;

                // First-rank utility is granted directly as part of prestige.
                if (newRank == 1)
                {
                    GrantPrestigeItem(player, ITEM_TOME_FLIGHT_PATHS, overflowRewards);
                    ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[Prestige]|r Received a Tome of Flight Paths. Use it to unlock every flight path.");
                }

                // Reward 2: 28-Slot Bag (Only up to Prestige 4, maximum of 4 bags)
                if (newRank <= 4)
                {
                    GrantPrestigeItem(player, ITEM_PRESTIGE_BAG, overflowRewards);
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[Prestige]|r Received 28-Slot Bag ({}/4): Satchel of the Ascended!", newRank);
                }

                // Reward 3: Random usable mount
                if (!PrestigeEpicMounts.empty())
                {
                    GrantPrestigeItem(player, chosenMount, overflowRewards);
                    if (ItemTemplate const* mProto = sObjectMgr->GetItemTemplate(chosenMount))
                    {
                        ChatHandler(player->GetSession()).PSendSysMessage("|cffa335ee[Prestige Mount]|r Received rare epic mount: [{}]!", mProto->Name1);
                    }
                }

                // Reward 4: FULL CLASS VANITY SET (ALL 8 PIECES AT ONCE) + Ascension Vanity
                uint8 chrClass = player->getClass();
                if (newRank <= 4)
                {
                    std::vector<uint32> fullSet = GetClassVanitySet(chrClass, newRank);
                    for (uint32 itemId : fullSet)
                    {
                        if (itemId != 0)
                            GrantPrestigeItem(player, itemId, overflowRewards);
                    }
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffa335ee[Prestige Transmog]|r Granted the FULL 8-piece class vanity armor set!");

                    if (newRank == 1)
                    {
                        GrantPrestigeItem(player, ITEM_PRESTIGE_TABARD, overflowRewards);
                        SetPrestigeTitle(player, 150); // the Undaunted
                    }
                    else if (newRank == 2)
                    {
                        GrantPrestigeItem(player, ITEM_PRESTIGE_CLOAK, overflowRewards);
                        SetPrestigeTitle(player, 141); // the Noble
                    }
                    else if (newRank == 3)
                    {
                        if (chrClass == CLASS_WARRIOR || chrClass == CLASS_PALADIN || chrClass == CLASS_DEATH_KNIGHT || chrClass == CLASS_HUNTER)
                            GrantPrestigeItem(player, ITEM_PRESTIGE_SWORD, overflowRewards);
                        else
                            GrantPrestigeItem(player, ITEM_PRESTIGE_SCYTHE, overflowRewards);
                        SetPrestigeTitle(player, 151); // the Argent Champion
                    }
                    else if (newRank == 4)
                    {
                        GrantPrestigeItem(player, ITEM_PRESTIGE_HELM, overflowRewards); // Crown of the Ascended
                        SetPrestigeTitle(player, 124); // the Exalted
                    }
                }
                else if (newRank >= 5)
                {
                    GrantPrestigeItem(player, ITEM_PRESTIGE_PARAGON_H, overflowRewards); // Helm of the Eternal Paragon
                    GrantPrestigeItem(player, ITEM_PRESTIGE_SHOULDER, overflowRewards);  // Pauldrons of the Eternal Paragon
                    GrantPrestigeItem(player, ITEM_PRESTIGE_VOIDBLADE, overflowRewards); // Blade of the Black Empire
                    GrantPrestigeItem(player, ITEM_PRESTIGE_SHIELD, overflowRewards);    // Shield of the Eternal Watcher
                    SetPrestigeTitle(player, 142); // the Immortal
                }

                if (!overflowRewards.empty())
                {
                    player->SendItemRetrievalMail(overflowRewards);
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffffff00[Prestige]|r {} reward item(s) were sent by Postmaster mail because your bags could not hold them.", overflowRewards.size());
                }

                // Visual effect: Holy Resurrection Ascended flare
                player->CastSpell(player, 24312, true);

                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff[Prestige]|r Congratulations! You have achieved |cffff8000Prestige Rank {}|r! Your rank rewards have been granted.", newRank);

                if (sConfigMgr->GetOption<bool>("Prestige.AnnounceGlobal", true))
                {
                    std::string msg = "|cff00ffff[Prestige]|r Champion |cffff8000" + player->GetName() + "|r has achieved Prestige Rank " + std::to_string(newRank) + "!";
                    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, msg);
                }

                return true;
            }
        }
        CloseGossipMenuFor(player);
        return true;
    }
};

class prestige_commandscript : public CommandScript
{
public:
    prestige_commandscript() : CommandScript("prestige_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable prestigeSubCommands =
        {
            { "info", HandlePrestigeInfo, SEC_PLAYER, Console::No },
            { "",     HandlePrestigeInfo, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "prestige", prestigeSubCommands }
        };

        return commandTable;
    }

    static bool HandlePrestigeInfo(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 rank = npc_prestige_master::GetPrestigeRank(player);
        handler->PSendSysMessage("|cff00ffff=== Prestige Character Status ===|r");
        handler->PSendSysMessage("Current Prestige Rank: |cff00ff00{}|r", rank);
        handler->PSendSysMessage("Reach level 80, then visit the Master of Prestige in Stormwind, Orgrimmar, or Dalaran.");
        return true;
    }
};

void AddSC_mod_prestige()
{
    new npc_prestige_master();
    new prestige_commandscript();
    new item_tome_of_flightpaths();
}

void Addmod_prestigeScripts()
{
    AddSC_mod_prestige();
}
