#ifndef CAPSTONE_SHADOW_REFLECTION_TIMELINE_H
#define CAPSTONE_SHADOW_REFLECTION_TIMELINE_H

#include <cstdint>
#include <deque>
#include <optional>

namespace CapstoneTalents
{
struct ReflectionAction
{
    std::uint32_t SpellId;
    std::uint32_t RecordedAtMs;
    std::uint8_t ComboPoints;
};

// Times are measured from the original cast, not from the previous update.
class ShadowReflectionTimeline
{
public:
    static constexpr std::uint32_t RecordingDurationMs = 8000;
    static constexpr std::uint32_t ReplayDelayMs = 8000;
    static constexpr std::uint32_t TotalDurationMs = 16000;

    bool Record(std::uint32_t elapsedMs, std::uint32_t spellId, std::uint8_t comboPoints)
    {
        if (elapsedMs >= RecordingDurationMs || !spellId || comboPoints > 5 ||
            (!_actions.empty() && elapsedMs < _actions.back().RecordedAtMs))
            return false;

        _actions.push_back({spellId, elapsedMs, comboPoints});
        return true;
    }

    std::optional<ReflectionAction> PopDue(std::uint32_t elapsedMs)
    {
        if (_actions.empty() || elapsedMs < _actions.front().RecordedAtMs + ReplayDelayMs)
            return std::nullopt;

        ReflectionAction const action = _actions.front();
        _actions.pop_front();
        return action;
    }

private:
    std::deque<ReflectionAction> _actions;
};
}

#endif
