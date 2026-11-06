/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GuildValues.h"
#include "AdvaiMgr.h"
#include "Playerbots.h"

uint8 PetitionSignsValue::Calculate()
{
    if (bot->GetGuildId())
        return 0;
    if (Advai::Manager::Instance().Applies(bot))
        return Advai::Manager::Instance().PetitionSignatures(bot->GetGUID().GetCounter());

    std::vector<Item*> petitions = AI_VALUE2(std::vector<Item*>, "inventory items", chat->FormatQItem(5863));
    if (petitions.empty())
        return 0;

    QueryResult result = CharacterDatabase.Query("SELECT playerguid FROM petition_sign WHERE petitionguid = {}",
                                                 petitions.front()->GetGUID().GetCounter());
    return result ? (uint8)result->GetRowCount() : 0;
}
