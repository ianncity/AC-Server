#include "ScriptMgr.h"
#include "Player.h"
#include "Pet.h"
#include <algorithm>
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "../../AscensionHookGuard.h"
#include "OverrankTalentData.h"
#include "SpellMgr.h"
#include "../../AscensionTalentSourceSpells.h"
#include <charconv>
#include <cctype>
#include <memory>
#include <mutex>
#include <sstream>
#include <string_view>
#include <unordered_set>

using namespace Acore::ChatCommands;

struct PlayerOverrankState
{
    std::mutex mutex;
    std::unordered_set<uint32> overrankedTalents;
    std::vector<SpellModifier*> activeSpellMods;
};

static std::unordered_map<ObjectGuid, std::shared_ptr<PlayerOverrankState>> s_PlayerOverrankMap;
static std::mutex s_PlayerOverrankMapMutex;
static std::unordered_set<SpellModifier const*> s_OverrankSpellMods;
static std::mutex s_OverrankSpellModsMutex;

static void AddOverrankSpellMod(Player* player, SpellModifier* mod, std::vector<SpellModifier*>& trackedMods)
{
    {
        std::lock_guard<std::mutex> lock(s_OverrankSpellModsMutex);
        s_OverrankSpellMods.insert(mod);
    }
    trackedMods.push_back(mod);
    player->AddSpellMod(mod, true);
}

static std::shared_ptr<PlayerOverrankState> GetPlayerOverrankState(ObjectGuid guid)
{
    std::lock_guard<std::mutex> lock(s_PlayerOverrankMapMutex);
    auto it = s_PlayerOverrankMap.find(guid);
    if (it != s_PlayerOverrankMap.end())
        return it->second;

    auto state = std::make_shared<PlayerOverrankState>();
    s_PlayerOverrankMap.emplace(guid, state);
    return state;
}

static void ForgetPlayerOverrankState(ObjectGuid guid)
{
    std::lock_guard<std::mutex> lock(s_PlayerOverrankMapMutex);
    s_PlayerOverrankMap.erase(guid);
}

static std::vector<uint32> GetOverrankedTalentIds(ObjectGuid guid)
{
    std::shared_ptr<PlayerOverrankState> state = GetPlayerOverrankState(guid);
    std::lock_guard<std::mutex> lock(state->mutex);
    return std::vector<uint32>(state->overrankedTalents.begin(), state->overrankedTalents.end());
}

static bool IsTalentOverrankEnabled()
{
    static bool const enabled = sConfigMgr->GetOption<bool>("TalentOverrank.Enable", true);
    return enabled;
}

static bool IsTalentOverrankAnnounceEnabled()
{
    static bool const enabled = sConfigMgr->GetOption<bool>("TalentOverrank.Announce", true);
    return enabled;
}

static uint32 GetLevel30ShardCount()
{
    static uint32 const count = sConfigMgr->GetOption<uint32>("TalentOverrank.Level30Shards", 1);
    return count;
}

static uint32 GetLevel60ShardCount()
{
    static uint32 const count = sConfigMgr->GetOption<uint32>("TalentOverrank.Level60Shards", 2);
    return count;
}

static bool ParseTalentId(std::string_view text, uint32& talentId)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);

    if (text.empty())
        return false;

    uint32 parsed = 0;
    auto const result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return false;

    talentId = parsed;
    return true;
}

// Helper: Calculate total allowed shards based on player level
static uint32 GetTotalShardsForLevel(uint8 level)
{
    if (!IsTalentOverrankEnabled())
        return 0;

    if (level >= 60)
        return GetLevel60ShardCount();
    if (level >= 30)
        return GetLevel30ShardCount();
    return 0;
}

// Helper: Send addon message to player client
static void SendOverrankAddonMessage(Player* player, std::string const& payload)
{
    if (!player || !player->GetSession())
        return;

    std::string msg = "OVERRANK\t" + payload;
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player, player, msg);
    player->SendDirectMessage(&data);
}

// Helper: Send full sync packet of shards and overranked talents
static void SyncOverrankState(Player* player)
{
    if (!player)
        return;

    uint32 total = GetTotalShardsForLevel(player->GetLevel());
    std::vector<uint32> overrankedTalents = GetOverrankedTalentIds(player->GetGUID());

    uint32 spent = static_cast<uint32>(overrankedTalents.size());
    uint32 available = total >= spent ? (total - spent) : 0;

    std::ostringstream ss;
    ss << "SYNC:" << available << ":" << total << ":";
    bool first = true;
    for (uint32 tid : overrankedTalents)
    {
        if (!first)
            ss << ",";
        ss << tid;
        first = false;
    }

    SendOverrankAddonMessage(player, ss.str());
}

// Helper: Apply or unapply overrank buff for a specific talent
// Custom-class talents are rank markers (998xxx, no spell family) that grant real source spells. A spell
// modifier only applies to spells of its owner's family whose flags overlap its mask, so build overrank
// modifiers from the granted spell instead of the marker; otherwise they silently never apply.
static void ResolveOverrankModTarget(OverrankTalentInfo const& info, uint32& spellId, flag96& mask)
{
    spellId = info.highestSpellId;
    mask = flag96(info.spellFamilyFlags[0], info.spellFamilyFlags[1], info.spellFamilyFlags[2]);
    using namespace AscensionTalentSources;
    if (info.talentId < FirstTalentId || info.talentId >= FirstTalentId + TalentCount)
        return;
    auto const& row = TalentSpellIds[info.talentId - FirstTalentId];
    for (int rank = MaxTalentRanks - 1; rank >= 0; --rank)
        if (row[rank])
        {
            if (SpellInfo const* target = sSpellMgr->GetSpellInfo(row[rank]))
            {
                spellId = target->Id;
                if (!mask)
                    mask = target->SpellFamilyFlags;
            }
            return;
        }
}

static void ApplyOverrankBuff(Player* player, OverrankTalentInfo const& info, bool apply, std::vector<SpellModifier*>& trackedMods)
{
    if (!player)
        return;

    player->SetTalentOverranked(info.talentId, apply);

    switch (info.effectType)
    {
        case OVERRANK_SPELLMOD:
        {
            if (apply)
            {
                uint32 targetSpell;
                flag96 targetMask;
                ResolveOverrankModTarget(info, targetSpell, targetMask);
                SpellModifier* mod = new SpellModifier();
                mod->op = (SpellModOp)info.spellModOp;
                mod->type = (SpellModType)info.spellModType;
                mod->value = info.spellModValue;
                mod->mask = targetMask;
                mod->spellId = targetSpell;
                AddOverrankSpellMod(player, mod, trackedMods);
            }
            break;
        }
        case OVERRANK_ACTIVE_POWER:
        {
            if (apply)
            {
                uint32 targetSpell;
                flag96 targetMask;
                ResolveOverrankModTarget(info, targetSpell, targetMask);
                // +15% damage/healing mod
                SpellModifier* modDmg = new SpellModifier();
                modDmg->op = SPELLMOD_DAMAGE;
                modDmg->type = SPELLMOD_PCT;
                modDmg->value = 15;
                modDmg->mask = targetMask;
                modDmg->spellId = targetSpell;
                AddOverrankSpellMod(player, modDmg, trackedMods);

                // -15% resource cost mod
                SpellModifier* modCost = new SpellModifier();
                modCost->op = SPELLMOD_COST;
                modCost->type = SPELLMOD_PCT;
                modCost->value = -15;
                modCost->mask = targetMask;
                modCost->spellId = targetSpell;
                AddOverrankSpellMod(player, modCost, trackedMods);
            }
            break;
        }
        case OVERRANK_RATING:
        {
            player->ApplyRatingMod((CombatRating)info.combatRating, info.ratingValue, apply);
            if (info.combatRating == CR_CRIT_MELEE)
                player->ApplyRatingMod(CR_CRIT_SPELL, info.ratingValue, apply);
            break;
        }
        case OVERRANK_STAT:
        {
            player->ApplyStatPctModifier((UnitMods)info.unitMod, TOTAL_PCT, apply ? info.statPct : -info.statPct);
            break;
        }
        case OVERRANK_SPELL_POWER:
        {
            player->ApplySpellPowerBonus(info.spellModValue, apply);
            break;
        }
        case OVERRANK_MINION_BUFF:
        {
            // Dynamically checked by custom summons via player->IsTalentOverranked(info.talentId)
            // Also apply to standard pet if currently spawned
            if (Pet* pet = player->GetPet())
            {
                pet->ApplyStatPctModifier(UNIT_MOD_HEALTH, TOTAL_PCT, apply ? 0.10f : -0.10f);
                pet->ApplyStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, apply ? 0.10f : -0.10f);
                pet->ApplyStatPctModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, apply ? 0.10f : -0.10f);
                // The health modifier above already rescales max health; scaling it again doubled the bonus.
                if (!apply)
                    pet->SetHealth(std::min(pet->GetHealth(), pet->GetMaxHealth()));
            }
            break;
        }
        case OVERRANK_GENERIC_BONUS:
        {
            uint32 targetSpell;
            flag96 targetMask;
            ResolveOverrankModTarget(info, targetSpell, targetMask);
            if (apply && targetMask)
            {
                SpellModifier* mod = new SpellModifier();
                mod->op = SPELLMOD_ALL_EFFECTS;
                mod->type = SPELLMOD_PCT;
                mod->value = 10;
                mod->mask = targetMask;
                mod->spellId = targetSpell;
                AddOverrankSpellMod(player, mod, trackedMods);
            }
            break;
        }
        default:
            break;
    }
}

// Helper: Clear all active overrank buffs from player
static void ClearActiveOverranksLocked(Player* player, PlayerOverrankState& state)
{
    auto const& dataMap = GetOverrankTalentData();

    // Remove spell mods (AddSpellMod(mod, false) automatically cleans up mod memory when ownerAura is null)
    for (SpellModifier* mod : state.activeSpellMods)
    {
        if (mod)
        {
            {
                std::lock_guard<std::mutex> lock(s_OverrankSpellModsMutex);
                s_OverrankSpellMods.erase(mod);
            }
            player->AddSpellMod(mod, false);
        }
    }
    state.activeSpellMods.clear();

    // Revert stat/rating mods
    for (uint32 tid : state.overrankedTalents)
    {
        auto dit = dataMap.find(tid);
        if (dit != dataMap.end())
        {
            std::vector<SpellModifier*> dummy;
            if (dit->second.effectType == OVERRANK_RATING ||
                dit->second.effectType == OVERRANK_STAT ||
                dit->second.effectType == OVERRANK_SPELL_POWER ||
                dit->second.effectType == OVERRANK_MINION_BUFF ||
                (dit->second.effectType == OVERRANK_GENERIC_BONUS && dit->second.spellFamilyFlags[0] == 0 && dit->second.spellFamilyFlags[1] == 0 && dit->second.spellFamilyFlags[2] == 0))
            {
                ApplyOverrankBuff(player, dit->second, false, dummy);
            }
        }
    }

    player->ClearOverrankedTalents();
    state.overrankedTalents.clear();
}

static void ClearActiveOverranks(Player* player)
{
    if (!player)
        return;

    std::shared_ptr<PlayerOverrankState> state = GetPlayerOverrankState(player->GetGUID());
    std::lock_guard<std::mutex> lock(state->mutex);
    ClearActiveOverranksLocked(player, *state);
}

static void ResetPlayerOverranks(Player* player, uint8 spec)
{
    if (!player)
        return;

    std::shared_ptr<PlayerOverrankState> state = GetPlayerOverrankState(player->GetGUID());
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        ClearActiveOverranksLocked(player, *state);
        CharacterDatabase.DirectExecute("DELETE FROM character_overranked_talents WHERE guid = {} AND spec = {}", player->GetGUID().GetCounter(), spec);
    }
}

// Helper: Load overranked talents from database for player's active spec
static void LoadPlayerOverranks(Player* player)
{
    if (!player)
        return;

    std::shared_ptr<PlayerOverrankState> state = GetPlayerOverrankState(player->GetGUID());
    std::unique_lock<std::mutex> lock(state->mutex);
    ClearActiveOverranksLocked(player, *state);

    uint32 guid = player->GetGUID().GetCounter();
    uint8 spec = player->GetActiveSpec();
    uint32 totalShards = GetTotalShardsForLevel(player->GetLevel());

    QueryResult result = CharacterDatabase.Query("SELECT talent_id FROM character_overranked_talents WHERE guid = {} AND spec = {}", guid, spec);
    auto const& dataMap = GetOverrankTalentData();

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 tid = fields[0].Get<uint32>();

            if (state->overrankedTalents.size() >= totalShards)
                break;

            auto dit = dataMap.find(tid);
            if (dit != dataMap.end())
            {
                TalentEntry const* talentEntry = sTalentStore.LookupEntry(tid);
                if (talentEntry)
                {
                    // Check if player still has the max rank learned in active spec
                    uint8 maxRank = dit->second.maxRank;
                    if (maxRank > 0 && maxRank <= 5 && talentEntry->RankID[maxRank - 1])
                    {
                        if (player->HasTalent(talentEntry->RankID[maxRank - 1], spec))
                        {
                            state->overrankedTalents.insert(tid);
                            ApplyOverrankBuff(player, dit->second, true, state->activeSpellMods);
                        }
                    }
                }
            }
        } while (result->NextRow());
    }

    lock.unlock();
    SyncOverrankState(player);
}

// Core: Process an Overrank attempt for a talent
static bool TryOverrankTalent(Player* player, uint32 talentId)
{
    if (!player)
        return false;

    if (!IsTalentOverrankEnabled())
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020Talent Overranking is currently disabled.|r");
        return false;
    }

    uint32 totalShards = GetTotalShardsForLevel(player->GetLevel());
    if (totalShards == 0)
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020You must be at least level 30 to earn Talent Shards.|r");
        return false;
    }

    std::shared_ptr<PlayerOverrankState> state = GetPlayerOverrankState(player->GetGUID());
    std::unique_lock<std::mutex> stateLock(state->mutex);
    if (state->overrankedTalents.size() >= totalShards)
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020You do not have any available Talent Shards! (Level 30: 1 shard, Level 60: 2 shards)|r");
        return false;
    }

    if (state->overrankedTalents.find(talentId) != state->overrankedTalents.end())
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020This talent is already overranked!|r");
        return false;
    }

    auto const& dataMap = GetOverrankTalentData();
    auto dit = dataMap.find(talentId);
    if (dit == dataMap.end())
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020Invalid talent for overranking.|r");
        return false;
    }

    if (dit->second.effectType == OVERRANK_DISALLOWED)
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020This talent cannot be enhanced with a Talent Shard.|r");
        return false;
    }

    TalentEntry const* talentEntry = sTalentStore.LookupEntry(talentId);
    if (!talentEntry)
        return false;

    TalentTabEntry const* tabEntry = sTalentTabStore.LookupEntry(talentEntry->TalentTab);
    if (!tabEntry || (tabEntry->ClassMask & player->getClassMask()) == 0)
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020This talent does not belong to your class.|r");
        return false;
    }

    uint8 maxRank = dit->second.maxRank;
    if (maxRank == 0 || maxRank > 5 || !talentEntry->RankID[maxRank - 1])
        return false;

    // Check if player has max rank learned in active spec
    if (!player->HasTalent(talentEntry->RankID[maxRank - 1], player->GetActiveSpec()))
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cffff2020You must learn the maximum rank of this talent before overranking it!|r");
        return false;
    }

    // Success! Save to DB
    uint32 guid = player->GetGUID().GetCounter();
    uint8 spec = player->GetActiveSpec();

    CharacterDatabase.DirectExecute("INSERT INTO character_overranked_talents (guid, talent_id, spec) VALUES ({}, {}, {})", guid, talentId, spec);

    state->overrankedTalents.insert(talentId);
    ApplyOverrankBuff(player, dit->second, true, state->activeSpellMods);
    stateLock.unlock();

    if (IsTalentOverrankAnnounceEnabled())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("|cffff8000[Talent Overranked!]|r |cff00ffff{}|r is now Overranked to Rank {}/{}! {}",
            dit->second.name, maxRank + 1, maxRank, dit->second.overrankDesc);
    }

    SyncOverrankState(player);
    return true;
}

// Addon Message handler
static void HandleOverrankClientMessage(Player* player, std::string const& body)
{
    if (!player)
        return;

    if (body == "QUERY")
    {
        SyncOverrankState(player);
    }
    else if (body.rfind("LEARN:", 0) == 0)
    {
        uint32 tid = 0;
        if (!ParseTalentId(std::string_view(body).substr(6), tid))
            return;

        TryOverrankTalent(player, tid);
    }
}

class mod_talent_overrank_playerScript : public PlayerScript
{
public:
    mod_talent_overrank_playerScript() : PlayerScript("mod_talent_overrank_playerScript") {}

    void OnPlayerLogin(Player* player) override
    {
        ASCENSION_GUARD("talent_overrank::OnPlayerLogin", [&] { LoginImpl(player); });
    }

    void LoginImpl(Player* player)
    {
        if (player)
        {
            LoadPlayerOverranks(player);
            if (IsTalentOverrankEnabled() && IsTalentOverrankAnnounceEnabled())
            {
                uint32 total = GetTotalShardsForLevel(player->GetLevel());
                uint32 spent = static_cast<uint32>(GetOverrankedTalentIds(player->GetGUID()).size());
                uint32 available = total >= spent ? (total - spent) : 0;
                ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff[Talent Shards]|r {} / {} Available. Open your Talent Tree to overrank maxed talents.", available, total);
            }
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
        {
            ClearActiveOverranks(player);
            ForgetPlayerOverrankState(player->GetGUID());
        }
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldlevel) override
    {
        if (!player)
            return;

        if (!IsTalentOverrankEnabled())
            return;

        uint8 newLevel = player->GetLevel();
        bool unlockedShard = false;
        if (oldlevel < 30 && newLevel >= 30)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffff8000[Talent Shard Unlocked!]|r You have reached level 30 and earned |cff00ffff1 Talent Shard|r! Open your Talent Tree to overrank a maxed talent.");
            unlockedShard = true;
        }
        if (oldlevel < 60 && newLevel >= 60)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffff8000[Talent Shard Unlocked!]|r You have reached level 60 and earned your |cff00ffff2nd Talent Shard|r! You can now overrank up to 2 talents.");
            unlockedShard = true;
        }

        if (unlockedShard)
            SyncOverrankState(player);
    }

    void OnPlayerTalentsReset(Player* player, bool /*noCost*/) override
    {
        if (!player)
            return;

        uint8 spec = player->GetActiveSpec();

        ResetPlayerOverranks(player, spec);

        ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[Talent Shards]|r All spent Talent Shards have been refunded!");
        SyncOverrankState(player);
    }

    void OnPlayerAfterGuardianInitStatsForLevel(Player* player, Guardian* guardian) override
    {
        if (!player || !guardian)
            return;

        auto const& dataMap = GetOverrankTalentData();
        for (auto const& pair : dataMap)
        {
            if (pair.second.effectType == OVERRANK_MINION_BUFF && player->IsTalentOverranked(pair.first))
            {
                guardian->ApplyStatPctModifier(UNIT_MOD_HEALTH, TOTAL_PCT, 0.10f);
                guardian->ApplyStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, 0.10f);
                guardian->ApplyStatPctModifier(UNIT_MOD_ATTACK_POWER, TOTAL_PCT, 0.10f);
                // Max health was already raised by the modifier above; refill to the new maximum.
                guardian->SetFullHealth();
            }
        }
    }

    void OnPlayerAfterSpecSlotChanged(Player* player, uint8 /*newSlot*/) override
    {
        if (player)
        {
            LoadPlayerOverranks(player);
        }
    }

    void OnPlayerBeforeSendChatMessage(Player* player, uint32& /*type*/, uint32& lang, std::string& msg) override
    {
        if (lang == LANG_ADDON && msg.rfind("OVERRANK\t", 0) == 0)
        {
            std::string body = msg.substr(9);
            HandleOverrankClientMessage(player, body);
            msg.clear();
        }
    }
};

class mod_talent_overrank_commandscript : public CommandScript
{
public:
    mod_talent_overrank_commandscript() : CommandScript("mod_talent_overrank_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable overrankSubCommands =
        {
            { "learn", HandleOverrankLearn, SEC_PLAYER, Console::No },
            { "reset", HandleOverrankReset, SEC_PLAYER, Console::No },
            { "info",  HandleOverrankInfo,  SEC_PLAYER, Console::No },
            { "",      HandleOverrankInfo,  SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "overrank", overrankSubCommands }
        };

        return commandTable;
    }

    static bool HandleOverrankLearn(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        if (!*args)
        {
            handler->SendSysMessage("Syntax: .overrank learn <talentId>");
            return true;
        }

        uint32 tid = 0;
        if (!ParseTalentId(args, tid))
        {
            handler->SendSysMessage("Invalid talent ID. Use .overrank learn <talentId>.");
            return true;
        }

        TryOverrankTalent(player, tid);
        return true;
    }

    static bool HandleOverrankReset(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint8 spec = player->GetActiveSpec();

        ResetPlayerOverranks(player, spec);

        handler->SendSysMessage("|cff00ff00[Talent Shards]|r All spent Talent Shards for this spec have been refunded.");
        SyncOverrankState(player);
        return true;
    }

    static bool HandleOverrankInfo(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        uint32 total = GetTotalShardsForLevel(player->GetLevel());
        std::vector<uint32> overrankedTalents = GetOverrankedTalentIds(player->GetGUID());
        uint32 spent = static_cast<uint32>(overrankedTalents.size());
        uint32 available = total >= spent ? (total - spent) : 0;

        handler->PSendSysMessage("|cffff8000=== Talent Overranking Status ===|r");
        handler->PSendSysMessage("Available Shards: |cff00ff00{}|r / |cff00ffff{}|r", available, total);
        handler->PSendSysMessage("Overranked Talents in active spec: {}", overrankedTalents.size());

        auto const& dataMap = GetOverrankTalentData();
        for (uint32 tid : overrankedTalents)
        {
            auto dit = dataMap.find(tid);
            if (dit != dataMap.end())
            {
                handler->PSendSysMessage(" - |cff00ffff{}|r (Rank {}/{}): {}",
                    dit->second.name, dit->second.maxRank + 1, dit->second.maxRank, dit->second.overrankDesc);
            }
        }
        return true;
    }
};

class mod_talent_overrank_globalScript : public GlobalScript
{
public:
    mod_talent_overrank_globalScript() : GlobalScript("mod_talent_overrank_globalScript") {}

    bool OnIsAffectedBySpellModCheck(SpellInfo const* /*affectSpell*/, SpellInfo const* checkSpell, SpellModifier const* mod) override
    {
        if (mod && !mod->ownerAura && mod->spellId && checkSpell && mod->spellId == checkSpell->Id)
        {
            std::lock_guard<std::mutex> lock(s_OverrankSpellModsMutex);
            if (s_OverrankSpellMods.count(mod))
                return false;
        }
        return true;
    }
};

void AddSC_mod_talent_overrank()
{
    new mod_talent_overrank_playerScript();
    new mod_talent_overrank_commandscript();
    new mod_talent_overrank_globalScript();
}

void Addmod_talent_overrankScripts()
{
    AddSC_mod_talent_overrank();
}
