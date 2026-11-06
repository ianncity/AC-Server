/*
 * Exception firewall for PlayerScript/WorldScript hooks that run on map-update or session threads.
 *
 * An exception (or failed ASSERT turned into a throw) that escapes Player::Update or the login handler
 * takes the whole realm down, so every custom hook body runs through AscensionGuard(). A failure is logged
 * at most once per hook per minute (so a per-tick failure cannot flood the log) and the tick is skipped.
 */
#ifndef ASCENSION_HOOK_GUARD_H
#define ASCENSION_HOOK_GUARD_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>

#include "Log.h"

namespace AscensionHookGuard
{
inline bool ShouldLog(std::atomic<int64_t>& lastLogged)
{
    int64_t const now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t previous = lastLogged.load(std::memory_order_relaxed);
    return (previous == 0 || now - previous >= 60) &&
        lastLogged.compare_exchange_strong(previous, now, std::memory_order_relaxed);
}
}  // namespace AscensionHookGuard

// Runs `body` (a callable) and swallows any exception. `hook` must be a string literal naming the hook.
#define ASCENSION_GUARD(hook, body)                                                                   \
    do                                                                                                \
    {                                                                                                 \
        static std::atomic<int64_t> guardLastLogged{0};                                               \
        try                                                                                           \
        {                                                                                             \
            body();                                                                                   \
        }                                                                                             \
        catch (std::exception const& guardError)                                                      \
        {                                                                                             \
            if (AscensionHookGuard::ShouldLog(guardLastLogged))                                       \
                LOG_ERROR("module", "Hook {} failed and was contained: {}", hook, guardError.what()); \
        }                                                                                             \
        catch (...)                                                                                   \
        {                                                                                             \
            if (AscensionHookGuard::ShouldLog(guardLastLogged))                                       \
                LOG_ERROR("module", "Hook {} failed with a non-standard exception and was contained.", hook); \
        }                                                                                             \
    } while (false)

#endif
