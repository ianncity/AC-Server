#ifndef CAPSTONE_COMBAT_MATH_H
#define CAPSTONE_COMBAT_MATH_H

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>

namespace CapstoneTalents
{
inline double ExecutionSentenceWeight(std::uint32_t tick)
{
    return tick >= 10 ? 5.0 * std::pow(1.1, 9.0) : std::pow(1.1, double(tick));
}

inline std::int32_t ExecutionSentenceAmount(std::int32_t base, std::uint32_t tick)
{
    return static_cast<std::int32_t>(std::clamp(std::max(0, base) * ExecutionSentenceWeight(tick),
        0.0, double(std::numeric_limits<std::int32_t>::max())));
}

inline double HaloMultiplier(double distance)
{
    double offset = (std::max(0.0, distance) - 25.0) / 2.0;
    return std::clamp(0.5 * std::pow(1.01, -std::pow(offset, 4.0)) + 0.1
        + 0.015 * std::max(0.0, distance), 0.0, 1.0);
}

constexpr bool HaloWaveCrosses(double distance, double reach, double previousRadius, double radius)
{
    return distance - reach <= radius && distance + reach >= previousRadius;
}

constexpr std::uint32_t FatalDamagePrevented(std::uint32_t damage, std::uint32_t health)
{
    return health && damage >= health ? damage - health + 1 : 0;
}

constexpr std::int32_t AddHealingDebt(std::int32_t current, std::uint32_t added)
{
    // Aura amounts are signed 32-bit values in the target engine.
    std::int64_t total = std::max<std::int64_t>(0, current) + added;
    return static_cast<std::int32_t>(std::min<std::int64_t>(total, std::numeric_limits<std::int32_t>::max()));
}

constexpr std::int32_t StormBoltDamage(std::int32_t damage, bool permanentStunImmunity)
{
    std::int64_t total = std::max<std::int64_t>(0, damage) * (permanentStunImmunity ? 4 : 1);
    return static_cast<std::int32_t>(std::min<std::int64_t>(total, std::numeric_limits<std::int32_t>::max()));
}

constexpr std::uint32_t ScaleRuneCooldown(std::uint32_t remaining, std::uint32_t bonusPercent, bool applying)
{
    std::uint64_t numerator = applying ? 100 : 100ULL + bonusPercent;
    std::uint64_t denominator = applying ? 100ULL + bonusPercent : 100;
    std::uint64_t result = (remaining * numerator + denominator - 1) / denominator;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(result, std::numeric_limits<std::uint32_t>::max()));
}

constexpr std::uint32_t RuneCorruptionDuration(std::uint32_t baseCooldown)
{
    // Wrath regenerates six runes in parallel; MoP has three regeneration lanes.
    return std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::uint64_t(baseCooldown) * 3 / 20));
}

constexpr std::uint32_t ExpectedSpiritShellHealing(std::uint32_t normalHeal, std::uint32_t criticalHeal,
    double criticalChance, std::uint32_t aegisPercent)
{
    double chance = std::clamp(criticalChance, 0.0, 100.0) / 100.0;
    double expected = normalHeal * (1.0 - chance)
        + criticalHeal * chance * (1.0 + aegisPercent / 100.0);
    return static_cast<std::uint32_t>(std::min(expected, double(std::numeric_limits<std::int32_t>::max())));
}

constexpr std::int32_t SpiritShellTotal(std::int32_t existing, std::uint32_t added, std::uint32_t casterMaxHealth)
{
    std::uint64_t cap = std::uint64_t(casterMaxHealth) * 60 / 100;
    return static_cast<std::int32_t>(std::min<std::uint64_t>(AddHealingDebt(existing, added), cap));
}

constexpr std::uint32_t HastedTimer(std::uint32_t baseTime, double castSpeedMultiplier)
{
    return static_cast<std::uint32_t>(std::clamp(baseTime * castSpeedMultiplier, 0.0,
        double(std::numeric_limits<std::uint32_t>::max())));
}

constexpr std::int32_t FrostBombDamage(std::int32_t damage, bool playerTarget)
{
    std::int64_t amount = std::max<std::int64_t>(0, damage);
    return static_cast<std::int32_t>(playerTarget ? amount * 70 / 100 : amount);
}
}

#endif
