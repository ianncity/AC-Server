// Item wardrobe transport for the ported Ascension model-grid interface.
// Every request is checked against the existing module's collection and policy.
#include "Transmogrification.h"
#include "Bag.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Creature.h"
#include "ScriptMgr.h"
#include "ObjectMgr.h"
#include "WorldPacket.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <map>
#include <sstream>

uint32 GetDracthyrDragonDisplay(Player const* player);
void ApplyDracthyrDisplay(Player* player, bool force);

namespace
{
constexpr char Prefix[] = "ACWARD1\t";
constexpr uint32 PageSize = 18;
constexpr uint8 Slots[] = {0, 2, 14, 4, 18, 3, 8, 9, 5, 6, 7, 17, 15, 16};
struct QuotedItem { ObjectGuid guid; uint32 price; };
struct Quote { std::map<uint8, QuotedItem> items; uint32 tokenEntry; uint32 tokenAmount; };
std::unordered_map<ObjectGuid::LowType, Quote> quotes;
bool Price(ItemTemplate const* item, uint32& price)
{
    auto* transmog = sTransmogrification;
    double raw = transmog->GetSpecialPrice(item) * static_cast<double>(transmog->GetScaledCostModifier()) + transmog->GetCopperCost();
    if (!std::isfinite(raw) || raw < 0 || raw > INT32_MAX) return false;
    price = static_cast<uint32>(raw);
    return true;
}
bool ValidSlot(uint32 slot) { return std::find(std::begin(Slots), std::end(Slots), slot) != std::end(Slots); }
bool Number(std::string const& text, uint32& value)
{
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && result.ec == std::errc() && result.ptr == text.data() + text.size();
}
std::vector<std::string> Split(std::string const& text, char delimiter)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    for (;;)
    {
        auto end = text.find(delimiter, start);
        result.push_back(text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}
std::string Lower(std::string text)
{
    for (char& value : text) value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    return text;
}
void Reply(Player* player, std::string const& payload)
{
    ASSERT(payload.size() + sizeof(Prefix) - 1 <= 255);
    WorldPacket packet;
    ChatHandler::BuildChatPacket(packet, CHAT_MSG_WHISPER, LANG_ADDON, player, player, std::string(Prefix) + payload);
    player->SendDirectMessage(&packet);
}
void Error(Player* player, uint32 request, char const* reason)
{
    Reply(player, "ERROR|" + std::to_string(request) + "|" + reason);
}
std::unordered_set<uint32> Owned(Player* player)
{
    auto* transmog = sTransmogrification;
    if (transmog->GetUseCollectionSystem())
    {
        auto it = transmog->collectionCache.find(player->GetSession()->GetAccountId());
        return it == transmog->collectionCache.end() ? std::unordered_set<uint32>{} : it->second;
    }
    std::unordered_set<uint32> entries;
    // Same source inventory scope as the module's existing gossip menu.
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot)) entries.insert(item->GetEntry());
    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        if (Bag* bag = player->GetBagByPos(slot))
            for (uint32 pos = 0; pos < bag->GetBagSize(); ++pos)
                if (Item* item = player->GetItemByPos(slot, pos)) entries.insert(item->GetEntry());
    return entries;
}
bool InCategory(ItemTemplate const& item, uint32 slot)
{
    switch (slot)
    {
        case 0: return item.InventoryType == INVTYPE_HEAD;
        case 2: return item.InventoryType == INVTYPE_SHOULDERS;
        case 3: return item.InventoryType == INVTYPE_BODY;
        case 4: return item.InventoryType == INVTYPE_CHEST || item.InventoryType == INVTYPE_ROBE;
        case 5: return item.InventoryType == INVTYPE_WAIST;
        case 6: return item.InventoryType == INVTYPE_LEGS;
        case 7: return item.InventoryType == INVTYPE_FEET;
        case 8: return item.InventoryType == INVTYPE_WRISTS;
        case 9: return item.InventoryType == INVTYPE_HANDS;
        case 14: return item.InventoryType == INVTYPE_CLOAK;
        case 15: return item.InventoryType == INVTYPE_WEAPON || item.InventoryType == INVTYPE_WEAPONMAINHAND || item.InventoryType == INVTYPE_2HWEAPON;
        case 16: return item.InventoryType == INVTYPE_WEAPON || item.InventoryType == INVTYPE_WEAPONOFFHAND || item.InventoryType == INVTYPE_SHIELD || item.InventoryType == INVTYPE_HOLDABLE;
        case 17: return item.InventoryType == INVTYPE_RANGED || item.InventoryType == INVTYPE_RANGEDRIGHT || item.InventoryType == INVTYPE_THROWN;
        case 18: return item.InventoryType == INVTYPE_TABARD;
        default: return false;
    }
}
bool Access(Player* player)
{
    return player && sTransmogrification->IsEnabled();
}
void State(Player* player, uint32 request)
{
    auto* transmog = sTransmogrification;
    Quote quote;
    quote.tokenEntry = transmog->GetTokenEntry();
    quote.tokenAmount = transmog->GetRequireToken() ? transmog->GetTokenAmount() : 0;
    for (uint8 slot : Slots)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            uint32 price;
            if (!Price(item->GetTemplate(), price)) return Error(player, request, "Invalid server transmog price configuration.");
            quote.items[slot] = {item->GetGUID(), price};
        }
    quotes[player->GetGUID().GetCounter()] = quote;
    Reply(player, "READY|" + std::to_string(request) + "|2|" + std::to_string(transmog->GetTokenEntry()) + "|" +
        std::to_string(transmog->GetRequireToken() ? transmog->GetTokenAmount() : 0) + "|" +
        std::to_string(transmog->GetAllowHiddenTransmog()) + "|" + std::to_string(transmog->GetHiddenTransmogIsFree()));
    for (uint8 slot : Slots)
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            Reply(player, "EQUIP|" + std::to_string(request) + "|" + std::to_string(slot) + "|" +
                std::to_string(item->GetEntry()) + "|" + std::to_string(transmog->GetFakeEntry(item->GetGUID())) + "|" + std::to_string(quote.items.at(slot).price));
    if (player->getRace() == 12 || player->getRace() == 13)
    {
        uint8 armor = player->GetByteValue(PLAYER_BYTES_2, PLAYER_BYTES_2_OFFSET_FACIAL_STYLE);
        Reply(player, "DRAGONARMOR|" + std::to_string(request) + "|" + std::to_string(armor));
    }
    Reply(player, "STATEEND|" + std::to_string(request));
}
// Item templates in one wardrobe category, built once per slot from the item store. Built lazily on first use, so it
// reflects the templates loaded at that point; a reload of item_template needs a restart to change the category lists.
std::vector<ItemTemplate const*> const& WardrobeCategory(uint32 slot)
{
    static std::unordered_map<uint32, std::vector<ItemTemplate const*>> cache;
    auto found = cache.find(slot);
    if (found != cache.end())
        return found->second;
    std::vector<ItemTemplate const*>& list = cache[slot];
    for (auto const& [id, item] : *sObjectMgr->GetItemTemplateStore())
        if (InCategory(item, slot))
            list.push_back(&item);
    return list;
}

void Page(Player* player, uint32 request, std::vector<std::string> const& args)
{
    uint32 slot, page, filter, sorting;
    if (args.size() != 7 || !Number(args[2], slot) || !ValidSlot(slot) || !Number(args[3], page) || !page ||
        !Number(args[4], filter) || filter > 2 || !Number(args[5], sorting) || sorting > 3 || args[6].size() > 50)
        return Error(player, request, "Invalid wardrobe category or filter.");
    auto* transmog = sTransmogrification;
    auto owned = Owned(player);
    Item* destination = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    std::string search = Lower(args[6]);
    std::vector<ItemTemplate const*> rows;
    uint32 collected = 0, total = 0;
    // Only the slot's category is scanned (cached); the per-player checks run on those rows only.
    for (ItemTemplate const* item : WardrobeCategory(slot))
    {
        if (!item->DisplayInfoID || !transmog->SuitableForTransmogrification(player, item)) continue;
        ++total;
        bool known = owned.contains(item->ItemId);
        if (known) ++collected;
        if ((filter == 1 && !known) || (filter == 2 && known) || (!search.empty() && Lower(item->Name1).find(search) == std::string::npos)) continue;
        rows.push_back(item);
    }
    // sorting: 0 = ID, 1 = name A-Z, 2 = name Z-A, 3 = collected first (then by name, then ID)
    std::sort(rows.begin(), rows.end(), [sorting, &owned](auto* a, auto* b)
    {
        if (sorting == 3)
        {
            bool const ownedA = owned.contains(a->ItemId), ownedB = owned.contains(b->ItemId);
            if (ownedA != ownedB) return ownedA;
            if (a->Name1 != b->Name1) return a->Name1 < b->Name1;
        }
        else if (sorting && a->Name1 != b->Name1) return sorting == 1 ? a->Name1 < b->Name1 : a->Name1 > b->Name1;
        return a->ItemId < b->ItemId;
    });
    uint32 pages = std::max(1U, static_cast<uint32>((rows.size() + PageSize - 1) / PageSize));
    page = std::min(page, pages);
    uint32 price = 0;
    if (destination)
    {
        if (!Price(destination->GetTemplate(), price)) return Error(player, request, "Invalid server transmog price configuration.");
    }
    Reply(player, "PAGE|" + std::to_string(request) + "|" + std::to_string(slot) + "|" + std::to_string(page) + "|" + std::to_string(pages) + "|" + std::to_string(collected) + "|" + std::to_string(total) + "|" + std::to_string(price));
    for (uint32 i = (page - 1) * PageSize; i < std::min(static_cast<uint32>(rows.size()), page * PageSize); ++i)
    {
        auto* item = rows[i];
        bool usable = destination && owned.contains(item->ItemId) && transmog->CanTransmogrifyItemWithItem(player, destination->GetTemplate(), item);
        Reply(player, "ITEM|" + std::to_string(request) + "|" + std::to_string(item->ItemId) + "|" + std::to_string(owned.contains(item->ItemId)) + "|" + std::to_string(usable));
    }
    Reply(player, "PAGEEND|" + std::to_string(request));
}
void Apply(Player* player, uint32 request, std::vector<std::string> const& args)
{
    if (args.size() != 3) return Error(player, request, "Invalid apply request.");
    auto* transmog = sTransmogrification;
    auto quote = quotes.find(player->GetGUID().GetCounter());
    if (quote == quotes.end()) return Error(player, request, "Refresh the wardrobe before applying appearances.");
    if (quote->second.tokenEntry != transmog->GetTokenEntry() || quote->second.tokenAmount != (transmog->GetRequireToken() ? transmog->GetTokenAmount() : 0))
        return Error(player, request, "Transmog token prices changed. Refresh the wardrobe.");
    auto owned = Owned(player);
    auto parts = Split(args[2], ',');
    if (parts.empty() || parts.size() > std::size(Slots)) return Error(player, request, "Invalid appearance count.");
    std::map<uint8, std::pair<Item*, uint32>> changes;
    uint64 money = 0, tokens = 0;
    for (auto const& part : parts)
    {
        auto pair = Split(part, ':');
        uint32 slot, entry;
        if (pair.size() != 2 || !Number(pair[0], slot) || !ValidSlot(slot) || !Number(pair[1], entry) || changes.contains(slot))
            return Error(player, request, "Invalid or duplicate appearance slot.");
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item) return Error(player, request, "An equipped item changed. Refresh the wardrobe.");
        auto quoted = quote->second.items.find(slot);
        if (quoted == quote->second.items.end() || quoted->second.guid != item->GetGUID())
            return Error(player, request, "An equipped item changed. Refresh the wardrobe.");
        changes[slot] = {item, entry};
        if (transmog->GetFakeEntry(item->GetGUID()) == entry) continue;
        if (!entry) continue; // Resetting a look is free, as in the existing module.
        if (entry == HIDDEN_ITEM_ID)
        {
            if (!transmog->GetAllowHiddenTransmog()) return Error(player, request, "Hidden appearances are disabled.");
            uint32 cost;
            if (!Price(item->GetTemplate(), cost) || cost != quoted->second.price)
                return Error(player, request, "Transmog prices changed. Refresh the wardrobe.");
            if (!transmog->GetHiddenTransmogIsFree()) money += cost;
            continue;
        }
        auto* source = sObjectMgr->GetItemTemplate(entry);
        if (!source || !owned.contains(entry) || !transmog->CanTransmogrifyItemWithItem(player, item->GetTemplate(), source))
            return Error(player, request, "An appearance is uncollected or incompatible with your equipment.");
        uint32 cost;
        if (!Price(item->GetTemplate(), cost) || cost != quoted->second.price)
            return Error(player, request, "Transmog prices changed. Refresh the wardrobe.");
        money += cost;
        if (transmog->GetRequireToken()) tokens += transmog->GetTokenAmount();
    }
    if (money > INT32_MAX || !player->HasEnoughMoney(static_cast<uint32>(money))) return Error(player, request, "Not enough money for all pending appearances.");
    if (tokens > UINT32_MAX || (tokens && !player->HasItemCount(transmog->GetTokenEntry(), tokens)))
        return Error(player, request, "Not enough transmog tokens for all pending appearances.");
    // No yields or additional validation after charging: this complete batch is
    // processed in one world-thread call, and appearance writes share a transaction.
    if (tokens) player->DestroyItemCount(transmog->GetTokenEntry(), tokens, true);
    if (money) player->ModifyMoney(-static_cast<int32>(money), false);
    auto transaction = CharacterDatabase.BeginTransaction();
    for (auto const& [slot, change] : changes)
    {
        auto [item, entry] = change;
        if (transmog->GetFakeEntry(item->GetGUID()) == entry) continue;
        if (entry)
        {
            transmog->SetFakeEntry(player, entry, slot, item, &transaction);
            item->UpdatePlayedTime(player);
            item->SetNotRefundable(player);
            item->ClearSoulboundTradeable(player);
        }
        else transmog->DeleteFakeEntry(player, slot, item, &transaction);
    }
    player->SaveInventoryAndGoldToDB(transaction);
    CharacterDatabase.CommitTransaction(transaction);
    Reply(player, "APPLIED|" + std::to_string(request));
    ApplyDracthyrDisplay(player, true);
    State(player, request);
}
class AscensionWardrobe final : public PlayerScript
{
public:
    AscensionWardrobe() : PlayerScript("AscensionWardrobe", {PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT, PLAYERHOOK_ON_LOGOUT}) { }
    void OnPlayerLogout(Player* player) override
    {
        lastRequest.erase(player->GetGUID().GetCounter());
        lastPageRequest.erase(player->GetGUID().GetCounter());
        quotes.erase(player->GetGUID().GetCounter());
    }
    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& message, Player* receiver) override
    {
        if (type != CHAT_MSG_WHISPER || language != LANG_ADDON || message.rfind(Prefix, 0) != 0) return true;
        if (receiver != player) return false;
        auto args = Split(message.substr(sizeof(Prefix) - 1), '|');
        uint32 request = 0;
        if (args.size() < 2 || !Number(args[1], request)) { Error(player, 0, "Malformed wardrobe request."); return false; }
        if (!Access(player)) { Error(player, request, "The wardrobe is disabled."); return false; }
        auto now = std::chrono::steady_clock::now();
        auto& last = lastRequest[player->GetGUID().GetCounter()];
        if (args[0] != "DRAGONARMOR" && (now - last < std::chrono::milliseconds(100))) { Error(player, request, "Wardrobe requests are arriving too quickly."); return false; }
        last = now;
        if (args[0] == "PAGE")
        {
            // A page scans a whole category, so it gets a stricter cooldown than other requests.
            auto& lastPage = lastPageRequest[player->GetGUID().GetCounter()];
            if (now - lastPage < std::chrono::milliseconds(250)) { Error(player, request, "Wardrobe pages are arriving too quickly."); return false; }
            lastPage = now;
        }
        if (args[0] == "HELLO" && args.size() == 2) State(player, request);
        else if (args[0] == "PAGE") Page(player, request, args);
        else if (args[0] == "APPLY") Apply(player, request, args);
        else if (args[0] == "DRAGONARMOR" && args.size() >= 3)
        {
            uint32 armor = 0;
            if (Number(args[2], armor) && armor < 74)
            {
                player->SetByteValue(PLAYER_BYTES_2, PLAYER_BYTES_2_OFFSET_FACIAL_STYLE, static_cast<uint8>(armor));
                ApplyDracthyrDisplay(player, true);
                Reply(player, "DRAGONARMOR|" + std::to_string(request) + "|" + std::to_string(armor));
            }
            else Error(player, request, "Invalid dragon armor style.");
        }
        else Error(player, request, "Unknown wardrobe operation.");
        return false;
    }
private:
    std::unordered_map<ObjectGuid::LowType, std::chrono::steady_clock::time_point> lastRequest;
    std::unordered_map<ObjectGuid::LowType, std::chrono::steady_clock::time_point> lastPageRequest;
};
}
// GM check: how many items per wardrobe category the character is offered, with the first few names, so class/race
// coverage can be verified for any online character. Usage: .wardrobecheck <name>
class AscensionWardrobeCheck final : public CommandScript
{
public:
    AscensionWardrobeCheck() : CommandScript("AscensionWardrobeCheck") { }
    Acore::ChatCommands::ChatCommandTable GetCommands() const override
    {
        static Acore::ChatCommands::ChatCommandTable table = {{"wardrobecheck", Handle, SEC_GAMEMASTER, Acore::ChatCommands::Console::Yes}};
        return table;
    }
    static bool Handle(ChatHandler* handler, char const* args)
    {
        Player* player = ObjectAccessor::FindPlayerByName(args ? args : "");
        if (!player) { handler->SendSysMessage("usage: .wardrobecheck <online character>"); return true; }
        auto* transmog = sTransmogrification;
        std::map<uint32, uint32> counts;
        for (auto const& [id, item] : *sObjectMgr->GetItemTemplateStore())
        {
            if (!item.DisplayInfoID || !transmog->SuitableForTransmogrification(player, &item)) continue;
            for (uint8 slot : Slots)
                if (InCategory(item, slot)) { ++counts[slot]; break; }
        }
        std::ostringstream out;
        out << player->GetName() << " race " << uint32(player->getRace()) << " class " << uint32(player->getClass()) << " level " << uint32(player->GetLevel()) << ":";
        for (uint8 slot : Slots) out << " s" << uint32(slot) << "=" << counts[slot];
        handler->SendSysMessage(out.str().c_str());
        return true;
    }
};
void AddSC_AscensionWardrobe()
{
    // Registered from more than one loader path; a command may only be defined once or the core asserts at startup.
    static bool const registered = []
    {
        new AscensionWardrobe();
        new AscensionWardrobeCheck();
        return true;
    }();
    (void)registered;
}
void OpenAscensionWardrobe(Player* player)
{
    if (!sTransmogrification->IsEnabled())
    {
        ChatHandler(player->GetSession()).SendNotification("The wardrobe is disabled.");
        return;
    }
    Reply(player, "OPEN|0");
}
