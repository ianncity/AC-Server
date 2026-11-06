/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ADVAI_METRICS_H
#define PLAYERBOTS_ADVAI_METRICS_H

#include <chrono>
#include <cstdint>
#include <string>

namespace Advai
{
class Metrics
{
public:
    static void WorldTick(std::uint32_t milliseconds);
    static void LimitMemory(std::uint32_t megabytes);
    static void SampleMemory();
    static bool MemoryAllowsLogin();
    static void Processing(bool world, std::uint64_t microseconds);
    static void InitialItemGrant(std::uint32_t units);
    static void InitialMoneyGrant(std::uint32_t copper);
    static void DeniedGrant();
    static void Population(std::uint32_t online, std::uint32_t humanZone, std::uint32_t mandatory,
                           std::uint32_t background, std::uint32_t changes, std::uint64_t longestWait);
    static std::string Summary();
};

class ProcessingSample
{
public:
    explicit ProcessingSample(bool world) : _world(world), _started(std::chrono::steady_clock::now()) {}
    ~ProcessingSample()
    {
        Metrics::Processing(_world, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                                                                   std::chrono::steady_clock::now() - _started)
                                                                   .count()));
    }

private:
    bool _world;
    std::chrono::steady_clock::time_point _started;
};
}  // namespace Advai

#endif
