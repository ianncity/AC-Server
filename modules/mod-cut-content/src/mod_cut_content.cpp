#include "ScriptMgr.h"
#include "Player.h"
#include "Creature.h"
#include "Chat.h"
#include "ScriptedGossip.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"

enum CutContentData
{
    NPC_CRYPT_KEEPER_OUTSIDE = 90001,
    NPC_FAIRBANKS_PURIFIER   = 4542,
    NPC_FAIRBANKS_GHOST      = 20348,

    ITEM_CORRUPTED_ASHBRINGER    = 22691,
    ITEM_CORRUPTED_ASHBRINGER_80 = 322691,
    ITEM_PURIFIED_ASHBRINGER     = 13262,

    GOSSIP_SENDER_PURIFY     = 2,
    GOSSIP_ACTION_PURIFY     = 100,

    SPELL_SHADOW_BOLT_VOLLEY = 15245,
    SPELL_FEAR               = 12542,
    SPELL_TORMENTING_SHADOW  = 22667,
    SPELL_HOLY_RESURRECTION  = 24312
};

class npc_karazhan_crypt_keeper : public CreatureScript
{
public:
    npc_karazhan_crypt_keeper() : CreatureScript("npc_karazhan_crypt_keeper") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (creature->GetEntry() == NPC_CRYPT_KEEPER_OUTSIDE)
        {
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "Browse recovered relics and class gear.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_TRADE);
        }

        SendGossipMenuFor(player, 90001, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (action == GOSSIP_ACTION_TRADE)
        {
            player->GetSession()->SendListInventory(creature->GetGUID());
            return true;
        }

        ClearGossipMenuFor(player);
        CloseGossipMenuFor(player);
        return true;
    }
};

class npc_ashbringer_purifier : public CreatureScript
{
public:
    npc_ashbringer_purifier() : CreatureScript("npc_ashbringer_purifier") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->HasItemCount(ITEM_CORRUPTED_ASHBRINGER, 1, true) || player->HasItemCount(ITEM_CORRUPTED_ASHBRINGER_80, 1, true))
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|cffff8000[Purify Blade]|r Cleanse the Corrupted Ashbringer in the Holy Light.", GOSSIP_SENDER_PURIFY, GOSSIP_ACTION_PURIFY);
        }

        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);
        CloseGossipMenuFor(player);

        if (action == GOSSIP_ACTION_PURIFY)
        {
            uint32 targetCorruptedItem = 0;
            if (player->HasItemCount(ITEM_CORRUPTED_ASHBRINGER_80, 1, true))
                targetCorruptedItem = ITEM_CORRUPTED_ASHBRINGER_80;
            else if (player->HasItemCount(ITEM_CORRUPTED_ASHBRINGER, 1, true))
                targetCorruptedItem = ITEM_CORRUPTED_ASHBRINGER;

            if (targetCorruptedItem != 0)
            {
                // Preflight the reward before consuming the corrupted blade; destroying it first
                // lost the item outright when the bags were full.
                ItemPosCountVec dest;
                InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_PURIFIED_ASHBRINGER, 1);
                if (msg == EQUIP_ERR_OK)
                {
                    player->DestroyItemCount(targetCorruptedItem, 1, true);
                    player->StoreNewItem(dest, ITEM_PURIFIED_ASHBRINGER, true);
                    creature->Yell("The shadow dissipates! By the Holy Light, the Ashbringer has been reborn!", LANG_UNIVERSAL);
                    creature->CastSpell(player, SPELL_HOLY_RESURRECTION, true);
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000[The Ashbringer]|r has been cleansed and added to your inventory!");
                }
                else
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000Your inventory is full! Make space before purifying.|r");
                }
            }
        }

        return true;
    }
};

class boss_tormentor_upside_down_sinners : public CreatureScript
{
public:
    boss_tormentor_upside_down_sinners() : CreatureScript("boss_tormentor_upside_down_sinners") {}

    struct boss_tormentor_upside_down_sinnersAI : public ScriptedAI
    {
        boss_tormentor_upside_down_sinnersAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 m_shadowBoltTimer;
        uint32 m_fearTimer;
        uint32 m_tormentTimer;

        void Reset() override
        {
            m_shadowBoltTimer = 4000;
            m_fearTimer = 14000;
            m_tormentTimer = 8000;
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            me->Yell("Who disturbs the eternal resting place of the forgotten?! You shall join the sinners below!", LANG_UNIVERSAL);
        }

        void JustDied(Unit* /*killer*/) override
        {
            me->Yell("The crypt... will never truly... be cleansed...", LANG_UNIVERSAL);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (m_shadowBoltTimer <= diff)
            {
                DoCastVictim(SPELL_SHADOW_BOLT_VOLLEY);
                m_shadowBoltTimer = urand(5000, 8000);
            }
            else
                m_shadowBoltTimer -= diff;

            if (m_fearTimer <= diff)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true))
                    DoCast(target, SPELL_FEAR);
                m_fearTimer = urand(16000, 22000);
            }
            else
                m_fearTimer -= diff;

            if (m_tormentTimer <= diff)
            {
                DoCastVictim(SPELL_TORMENTING_SHADOW);
                m_tormentTimer = urand(10000, 15000);
            }
            else
                m_tormentTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_tormentor_upside_down_sinnersAI(creature);
    }
};

void AddSC_mod_cut_content()
{
    new npc_karazhan_crypt_keeper();
    new npc_ashbringer_purifier();
    new boss_tormentor_upside_down_sinners();
}

void Addmod_cut_contentScripts()
{
    AddSC_mod_cut_content();
}
