/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_CATALOG_H
#define PLAYERBOTS_ADVAI_CATALOG_H

#include <cstdint>
#include <deque>
#include <initializer_list>
#include <memory>
#include <string>

namespace Advai
{
constexpr std::uint32_t TEXT_LOCALE = 0;  // English, independent of the realm's DBC locale.
struct BotPersona;
struct BotMood;
struct DialogueContext;
struct SpeechData;
using CatalogHandle = std::shared_ptr<SpeechData const>;

struct SpeechLine
{
    std::string Text;
    std::string Source;
    std::uint32_t Variant = 0;
    CatalogHandle Catalog;
};

CatalogHandle CurrentCatalog();
SpeechLine SelectCatalogSpeech(CatalogHandle const& catalog, std::string const& key, std::uint32_t locale,
                               std::uint32_t selector, std::deque<std::uint32_t> const& recent,
                               BotPersona const& persona, BotMood const& mood, DialogueContext const& context);
std::string CatalogFact(CatalogHandle const& catalog, std::string const& key, std::uint32_t locale,
                        std::initializer_list<std::string> values = {});
std::string CatalogLiteral(CatalogHandle const& catalog, std::string const& english, std::uint32_t locale);
}  // namespace Advai

#endif
