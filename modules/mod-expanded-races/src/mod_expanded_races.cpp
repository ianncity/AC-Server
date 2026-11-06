#include <algorithm>
#include <unordered_map>
#include <vector>
#include "AccountScript.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Chat.h"
#include "DataStores/DBCStores.h"
#include "Log.h"
#include "../../AscensionHookGuard.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "MovementHandlerScript.h"
#include "Server/Protocol/Opcodes.h"

using namespace Acore::ChatCommands;

namespace
{
constexpr uint32 SPELL_DRACTHYR_GLIDE = 363404;
constexpr uint32 SPELL_DRACONIC_MIGHT = 363407;
}

uint32 GetDracthyrDragonDisplay(Player const* player)
{
    uint8 const race = player->getRace();
    uint8 const gender = player->getGender();
    uint8 const armor = player->GetByteValue(PLAYER_BYTES_2, PLAYER_BYTES_2_OFFSET_FACIAL_STYLE);
    uint8 const hairStyle = player->GetByteValue(PLAYER_BYTES, PLAYER_BYTES_OFFSET_HAIR_STYLE_ID);
    if ((race != 12 && race != 13) || gender > GENDER_FEMALE || armor >= 74)
        return 0;
    uint32 const identity = uint32(race - 12) * 2 + gender;
    if (armor < 36)
    {
        return 50000 + identity;
    }

    return 51600 + uint32(armor - 36) * 8 + (hairStyle % 20 >= 10 ? 4 : 0) + identity;
}

// The race's own ChrRaces display (10011/10012 Alliance, 10013/10014 Horde) stays the NATIVE display. The 3.3.5 client
// draws a player from a creature display (the dragon armor models) only when the current display differs from the native
// one, exactly like a morph or druid form; setting both to the armor display made the client keep the plain race model, so
// the dragon armor arrows changed nothing visible. Armor is therefore shown as the current display only.
static bool IsDracthyrDisplay(uint32 display)
{
    return (display >= 10011 && display <= 10014) || (display >= 50000 && display <= 50003) ||
        (display >= 51000 && display <= 51566) || (display >= 51600 && display <= 51903);
}

void ApplyDracthyrDisplay(Player* player, bool force)
{
    uint8 const race = player->getRace();
    if (race != 12 && race != 13)
        return;
    uint32 const baseNative = 10011 + uint32(race - 12) * 2 + std::min<uint8>(player->getGender(), 1);
    uint32 const native = player->GetNativeDisplayId();
    if (native != baseNative && IsDracthyrDisplay(native))
        player->SetNativeDisplayId(baseNative);  // undo the earlier native promotion
    uint32 dragon = GetDracthyrDragonDisplay(player);
    if (!dragon || !sCreatureDisplayInfoStore.LookupEntry(dragon))
    {
        // An out-of-range armor byte (e.g. from a barbershop style) must not take the realm down from the per-tick
        // update; fall back to the bare dragon for this race/gender.
        uint32 const fallback = 50000 + uint32(race - 12) * 2 + std::min<uint8>(player->getGender(), 1);
        if (!force && player->GetDisplayId() != fallback)
            LOG_DEBUG("module", "Dracthyr {} (GUID {}) has no display for armor style {}; using {}.",
                      player->GetName(), player->GetGUID().GetCounter(),
                      player->GetByteValue(PLAYER_BYTES_2, PLAYER_BYTES_2_OFFSET_FACIAL_STYLE), fallback);
        dragon = fallback;
    }
    uint32 const shown = player->GetDisplayId();
    // Only replace a display we own; leave mounts, forms and transformations alone unless the player asked for a change.
    bool const transformed = player->IsMounted() || player->GetShapeshiftForm() != FORM_NONE;
    if (shown != dragon && ((force && !transformed) || IsDracthyrDisplay(shown)))
        player->SetDisplayId(dragon);
}

namespace
{

uint16 GetFactionLanguageSkill(uint8 race)
{
    switch (race)
    {
        case 1: // Human
        case 3: // Dwarf
        case 4: // Night Elf
        case 7: // Gnome
        case 11: // Draenei
        case 12: // Alliance Dracthyr
            return SKILL_LANG_COMMON;
        case 2: // Orc
        case 5: // Undead
        case 6: // Tauren
        case 8: // Troll
        case 10: // Blood Elf
        case 13: // Horde Dracthyr
            return SKILL_LANG_ORCISH;
        default:
            return 0;
    }
}

uint16 GetRacialLanguageSkill(uint8 race)
{
    switch (race)
    {
        case 3: return SKILL_LANG_DWARVEN;
        case 4: return SKILL_LANG_DARNASSIAN;
        case 5: return SKILL_LANG_GUTTERSPEAK;
        case 6: return SKILL_LANG_TAURAHE;
        case 7: return SKILL_LANG_GNOMISH;
        case 8: return SKILL_LANG_TROLL;
        case 10: return SKILL_LANG_THALASSIAN;
        case 11: return SKILL_LANG_DRAENEI;
        case 12:
        case 13:
            return SKILL_LANG_DRACONIC;
        default:
            return 0;
    }
}

uint32 GetLanguageSpell(uint16 skill)
{
    switch (skill)
    {
        case SKILL_LANG_ORCISH:      return 669;
        case SKILL_LANG_DARNASSIAN:  return 671;
        case SKILL_LANG_TAURAHE:     return 670;
        case SKILL_LANG_DWARVEN:     return 672;
        case SKILL_LANG_COMMON:      return 668;
        case SKILL_LANG_THALASSIAN:  return 813;
        case SKILL_LANG_DRACONIC:    return 814;
        case SKILL_LANG_GNOMISH:     return 7340;
        case SKILL_LANG_TROLL:       return 7341;
        case SKILL_LANG_GUTTERSPEAK: return 17737;
        case SKILL_LANG_DRAENEI:     return 29932;
        default:                     return 0;
    }
}

bool IsDisallowedClassCombination(uint8 race, uint8 playerClass)
{
    bool const restrictedRace = race == 3 || race == 10 || race == 11; // Dwarf, Blood Elf, Draenei
    bool const ascensionClass = playerClass == 12 || playerClass == 13; // Cultist, Necromancer
    bool const dracthyrRace = race == 12 || race == 13;
    bool const dracthyrClass = playerClass == 1 || playerClass == 3 || playerClass == 4 || playerClass == 5 ||
        playerClass == 6 || playerClass == 8 || playerClass == 9 || playerClass == 12 || playerClass == 13;
    return (restrictedRace && ascensionClass) || (race == 2 && playerClass == 12) || // Orc Cultist
        (dracthyrRace && !dracthyrClass);
}
}

class mod_expanded_races_worldscript : public WorldScript
{
public:
    mod_expanded_races_worldscript() : WorldScript("mod_expanded_races_worldscript") {}

    void OnStartup() override
    {
        LOG_INFO("server.loading", ">> Loaded Module: Expanded Race/Class Combinations (Human Hunter, Orc Mage, Dwarf Shaman, etc.)");
    }
};

class mod_expanded_races_accountscript : public AccountScript
{
public:
    mod_expanded_races_accountscript() : AccountScript("mod_expanded_races_accountscript",
        { ACCOUNTHOOK_CAN_ACCOUNT_CREATE_CHARACTER, ACCOUNTHOOK_CAN_ACCOUNT_LOGIN_CHARACTER }) {}

    bool CanAccountCreateCharacter(uint32 /*accountId*/, uint8 race, uint8 playerClass) override
    {
        // Keep the server-side check aligned with CharBaseInfo.dbc and the
        // playercreateinfo race/class restriction migration. Class/race IDs
        // are the WotLK DBC IDs used by the character-create packet.
        if (IsDisallowedClassCombination(race, playerClass))
        {
            LOG_WARN("network.opcode", "Blocked character creation for restricted race/class combination ({}/{}).", race, playerClass);
            return false;
        }

        return true;
    }

    bool CanAccountLoginCharacter(uint32 /*accountId*/, uint8 /*race*/, uint8 /*playerClass*/) override
    {
        // Existing characters are grandfathered: the creation gate above stops new
        // restricted combinations, but refusing login here locked out characters
        // that already exist on the realm (Dwarf/Blood Elf/Draenei Cultist and
        // Necromancer). Restricting logins needs an explicit migration decision.
        return true;
    }
};

class mod_expanded_races_playerScript : public PlayerScript
{
public:
    mod_expanded_races_playerScript() : PlayerScript("mod_expanded_races_playerScript") {}

    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        ASCENSION_GUARD("expanded_races::OnPlayerUpdate", [&] { UpdateImpl(player); });
    }

    void UpdateImpl(Player* player)
    {
        if (player->getRace() != 12 && player->getRace() != 13)
            return;

        if (!player->HasPlayerFlag(PLAYER_FLAGS_HIDE_HELM))
            player->SetPlayerFlag(PLAYER_FLAGS_HIDE_HELM);
        if (player->GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (EQUIPMENT_SLOT_HEAD * 2)) != 0)
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (EQUIPMENT_SLOT_HEAD * 2), 0);

        bool const falling = player->IsFalling() ||
            (player->GetUnitMovementFlags() & (MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR));
        if ((!falling || !player->IsAlive() || player->IsInWater() || player->IsMounted()) &&
            player->HasAura(SPELL_DRACTHYR_GLIDE))
        {
            player->RemoveAurasDueToSpell(SPELL_DRACTHYR_GLIDE);
        }

        ApplyDracthyrDisplay(player, false);
        // Visage is retired. Remove stale transform auras and keep players on
        // their normal display even if an old spell was saved.
        player->RemoveAurasDueToSpell(363402);
        player->RemoveAurasDueToSpell(363403);
        player->RemoveAurasDueToSpell(372014);
    }

    void OnPlayerAfterSetVisibleItemSlot(Player* player, uint8 slot, Item* /*item*/) override
    {
        if (!player)
            return;
        uint8 const race = player->getRace();
        if (race != 12 && race != 13)
            return;

        if (slot == EQUIPMENT_SLOT_HEAD)
        {
            player->SetPlayerFlag(PLAYER_FLAGS_HIDE_HELM);
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (EQUIPMENT_SLOT_HEAD * 2), 0);
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (EQUIPMENT_SLOT_HEAD * 2), 0);
        }
        else if (slot == EQUIPMENT_SLOT_CHEST)
        {
            ApplyDracthyrDisplay(player, true);
        }
    }

    void OnPlayerLogin(Player* player) override
    {
        ASCENSION_GUARD("expanded_races::OnPlayerLogin", [&] { LoginImpl(player); });
    }

    void LoginImpl(Player* player)
    {
        if (!player)
            return;

        // The 3.3.5 client only treats a language as known when the character has its "Language X" spell (668 Common,
        // 669 Orcish, ...). Characters created through bot tooling or migrated between accounts lacked them, so the
        // client garbled every same-faction message and refused to let them speak. Grant the race's language spells.
        {
            static std::unordered_map<uint8, std::vector<uint32>> const languageSpells = {
                {1, {668}}, {2, {669}}, {3, {668, 672}}, {4, {668, 671}}, {5, {669, 17737}}, {6, {669, 670}},
                {7, {668, 7340}}, {8, {669, 7341}}, {10, {669, 813}}, {11, {668, 29932}}, {12, {668}}, {13, {669}}};
            if (auto const known = languageSpells.find(player->getRace()); known != languageSpells.end())
                for (uint32 spell : known->second)
                    if (!player->HasSpell(spell) && sSpellMgr->GetSpellInfo(spell))
                        player->learnSpell(spell, false);
        }

        uint8 race = player->getRace();
        bool isDracthyr = race == 12 || race == 13;

        if (isDracthyr)
        {
            if (!player->HasSkill(2808))
                player->SetSkill(2808, 0, 1, 1);
            // Suppress helmet model on Dracthyr to prevent missing attachment box rendering
            player->SetPlayerFlag(PLAYER_FLAGS_HIDE_HELM);
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + (EQUIPMENT_SLOT_HEAD * 2), 0);
            player->SetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENCHANTMENT + (EQUIPMENT_SLOT_HEAD * 2), 0);
            // Remove Visage from existing characters and clear any persisted
            // transform. The dragon racial kit remains available.
            player->RemoveAurasDueToSpell(363402);
            player->RemoveAurasDueToSpell(363403);
            player->RemoveAurasDueToSpell(372014);
            if (player->HasSpell(363402))
                player->removeSpell(363402, player->GetActiveSpec(), true);
            if (player->HasSpell(363403))
                player->removeSpell(363403, player->GetActiveSpec(), true);
            player->RemoveAurasDueToSpell(SPELL_DRACTHYR_GLIDE);
            if (player->HasSpell(SPELL_DRACTHYR_GLIDE))
                player->removeSpell(SPELL_DRACTHYR_GLIDE, player->GetActiveSpec(), true);
            uint32 racialSpells[] = { SPELL_DRACONIC_MIGHT, 363405u, 363406u, 360022u };
            for (uint32 spell : racialSpells)
                if (!player->HasSpell(spell))
                    player->learnSpell(spell, false);
        }

        // A stored language skill does not guarantee that the client has the
        // matching language spell. Reconcile both for every existing character
        // so normal and expanded race/class combinations use the same valid
        // faction and racial languages.
        uint16 languageSkills[] = { GetFactionLanguageSkill(race), GetRacialLanguageSkill(race) };
        for (uint16 languageSkill : languageSkills)
        {
            if (!languageSkill)
                continue;

            if (!GetSkillRaceClassInfo(languageSkill, player->getRace(), player->getClass()))
                LOG_WARN("entities.player", "Custom race/class combination ({}/{}) has no SkillRaceClassInfo.dbc row for language skill {}; granting it directly.",
                    player->getRace(), player->getClass(), languageSkill);

            if (player->GetPureSkillValue(languageSkill) < 300 || player->GetPureMaxSkillValue(languageSkill) < 300)
            {
                player->SetSkill(languageSkill, 0, 300, 300);
                LOG_WARN("entities.player", "Restored missing language skill {} for character {} (race {}, class {}).",
                    languageSkill, player->GetName(), player->getRace(), player->getClass());
            }

            if (!player->HasSkill(languageSkill))
                LOG_ERROR("entities.player", "Character {} still lacks language skill {} after custom-race/class initialization.", player->GetName(), languageSkill);

            uint32 languageSpell = GetLanguageSpell(languageSkill);
            if (languageSpell && !player->HasSpell(languageSpell))
                player->learnSpell(languageSpell, false);

            if (languageSpell && !player->HasSpell(languageSpell))
                LOG_ERROR("entities.player", "Character {} still lacks language spell {} for skill {} after custom-race/class initialization.",
                    player->GetName(), languageSpell, languageSkill);
        }
    }
};

// The extra jump is the deliberate Glide activation input. This avoids firing
// the racial on ordinary jumps and lets the client initiate it while falling.
class mod_expanded_races_movement_script : public MovementHandlerScript
{
public:
    mod_expanded_races_movement_script() : MovementHandlerScript("mod_expanded_races_movement_script") { }

    void OnPlayerMove(Player* player, MovementInfo movementInfo, uint32 opcode) override
    {
        if (opcode != MSG_MOVE_JUMP || !player || (player->getRace() != 12 && player->getRace() != 13))
            return;
        // MovementHandler runs before the accepted packet is copied onto the
        // Player. Use the packet's fall state here; the previous server state
        // can still say grounded when the client sends its mid-air jump.
        bool const fallingInPacket = movementInfo.HasMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR) &&
            movementInfo.fallTime >= 250;
        if (!player->IsAlive() || (!player->IsFalling() && !fallingInPacket) || player->IsFlying() ||
            player->IsInWater() || player->IsMounted() || player->HasAura(SPELL_DRACTHYR_GLIDE))
            return;

        // Glide racial removed per user request
    }
};

class spell_dracthyr_chosen_identity : public SpellScriptLoader
{
public:
    spell_dracthyr_chosen_identity() : SpellScriptLoader("spell_dracthyr_chosen_identity") {}

    class spell_dracthyr_chosen_identity_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_dracthyr_chosen_identity_SpellScript);
        bool _cancel = false;

        SpellCastResult CheckRace()
        {
            Player* player = GetCaster()->ToPlayer();
            return player && (player->getRace() == 12 || player->getRace() == 13)
                ? SPELL_CAST_OK : SPELL_FAILED_BAD_TARGETS;
        }

        void RememberState() { _cancel = GetCaster()->HasAura(360022); }

        void Toggle(SpellEffIndex /*index*/)
        {
            if (_cancel)
            {
                PreventHitAura();
                PreventHitDefaultEffect(EFFECT_0);
                GetCaster()->RemoveAurasDueToSpell(360022);
            }
        }

        void Register() override
        {
            OnCheckCast += SpellCheckCastFn(spell_dracthyr_chosen_identity_SpellScript::CheckRace);
            BeforeCast += SpellCastFn(spell_dracthyr_chosen_identity_SpellScript::RememberState);
            OnEffectHitTarget += SpellEffectFn(spell_dracthyr_chosen_identity_SpellScript::Toggle, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
        }
    };

    SpellScript* GetSpellScript() const override { return new spell_dracthyr_chosen_identity_SpellScript(); }
};

class mod_expanded_races_commandscript : public CommandScript
{
public:
    mod_expanded_races_commandscript() : CommandScript("mod_expanded_races_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable comboCommandTable =
        {
            { "list", HandleCombosList, SEC_PLAYER, Console::No },
            { "",     HandleCombosList, SEC_PLAYER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "combos", comboCommandTable }
        };

        return commandTable;
    }

    static bool HandleCombosList(ChatHandler* handler, const char* /*args*/)
    {
        handler->PSendSysMessage("|cff00ffff=== Expanded Race & Class Combinations Active ===|r");
        handler->SendSysMessage("|cff00ff00Alliance:|r Human Hunter, Night Elf Mage, Dwarf Shaman, Dwarf Mage, Dwarf Warlock, Gnome Priest, Gnome Hunter, Draenei Rogue, Dracthyr Cultist/Necromancer");
        handler->SendSysMessage("|cff00ff00Horde:|r Orc Mage, Orc Priest, Undead Hunter, Undead Paladin, Tauren Paladin, Tauren Priest, Troll Druid, Troll Warlock, Blood Elf Warrior, Dracthyr Cultist/Necromancer");
        handler->SendSysMessage("Orcs cannot be Cultists; Dracthyr cannot be Druids. Other listed combinations have customized starting data.");
        return true;
    }
};

void Addmod_expanded_racesScripts()
{
    new mod_expanded_races_worldscript();
    new mod_expanded_races_accountscript();
    new mod_expanded_races_playerScript();
    new mod_expanded_races_movement_script();
    new mod_expanded_races_commandscript();
    new spell_dracthyr_chosen_identity();
}
