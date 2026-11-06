/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AdvaiMetrics.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <Windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <sys/resource.h>

#include <fstream>
#else
#include <cstdlib>
#endif

namespace Advai
{
namespace
{
std::mutex samplesMutex;
std::array<std::uint32_t, 4096> samples{};
std::size_t sampleCount = 0;
std::size_t sampleCursor = 0;
std::array<std::atomic<std::uint64_t>, 2> processingCount{};
std::array<std::atomic<std::uint64_t>, 2> processingTime{};
std::array<std::atomic<std::uint64_t>, 2> processingPeak{};
std::atomic<std::uint64_t> initialInventoryUnits{0};
std::atomic<std::uint64_t> initialMoneyCopper{0};
std::atomic<std::uint64_t> deniedGrants{0};
std::atomic<std::uint32_t> onlinePopulation{0};
std::atomic<std::uint32_t> humanZonePopulation{0};
std::atomic<std::uint32_t> mandatoryPopulation{0};
std::atomic<std::uint32_t> backgroundPopulation{0};
std::atomic<std::uint64_t> activityChanges{0};
std::atomic<std::uint64_t> longestBackgroundWait{0};
std::atomic<std::uint64_t> memoryLimit{0};
std::atomic<std::uint64_t> privateMemory{0};
std::atomic<std::uint64_t> residentMemory{0};
std::atomic<std::uint64_t> peakMemory{0};
#if defined(_WIN32)
HANDLE memoryJob = nullptr;
#endif
}  // namespace

void Metrics::LimitMemory(std::uint32_t megabytes)
{
    if (!megabytes || megabytes > 28000)
        throw std::runtime_error("ADVAI memory limit must be between one and 28000 decimal MB");
    auto bytes = static_cast<std::uint64_t>(megabytes) * 1000000;
    if (auto previous = memoryLimit.load(); previous)
    {
        if (previous != bytes)
            throw std::runtime_error("ADVAI memory limit cannot change while the realm is running");
        return;
    }
    // Charge the entire world process, including core-owned bot objects. This is a conservative
    // upper bound on module memory and is enforced by the OS before an allocation can exceed it.
#if defined(_WIN32)
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
        throw std::runtime_error("ADVAI could not create the memory-limit job: " + std::to_string(GetLastError()));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = static_cast<SIZE_T>(bytes);
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job, GetCurrentProcess()))
    {
        auto error = GetLastError();
        CloseHandle(job);
        throw std::runtime_error("ADVAI could not enforce its memory limit: " + std::to_string(error));
    }
    memoryJob = job;
#elif defined(__linux__)
    rlimit limits{};
    if (getrlimit(RLIMIT_DATA, &limits) != 0)
        throw std::runtime_error("ADVAI could not inspect the process data limit");
    limits.rlim_cur = std::min<rlim_t>(limits.rlim_max, static_cast<rlim_t>(bytes));
    if (setrlimit(RLIMIT_DATA, &limits) != 0)
        throw std::runtime_error("ADVAI could not enforce its process data limit");
#else
    throw std::runtime_error("ADVAI memory enforcement is implemented for Windows and Linux only");
#endif
    memoryLimit.store(bytes);
    SampleMemory();
    if (privateMemory.load() >= bytes)
        throw std::runtime_error("ADVAI process already exceeds the configured memory limit");
}

void Metrics::SampleMemory()
{
    if (!memoryLimit.load())
        return;
    std::uint64_t committed = 0, resident = 0;
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                              static_cast<DWORD>(sizeof(memory))))
        throw std::runtime_error("ADVAI process memory accounting failed");
    committed = memory.PrivateUsage;
    resident = memory.WorkingSetSize;
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    bool foundData = false, foundResident = false;
    while (std::getline(status, line))
    {
        if (line.starts_with("VmData:"))
        {
            committed = std::stoull(line.substr(7)) * 1024;
            foundData = true;
        }
        if (line.starts_with("VmRSS:"))
        {
            resident = std::stoull(line.substr(6)) * 1024;
            foundResident = true;
        }
    }
    if (!foundData || !foundResident)
        throw std::runtime_error("ADVAI process memory accounting failed");
#endif
    privateMemory.store(committed, std::memory_order_relaxed);
    residentMemory.store(resident, std::memory_order_relaxed);
    auto peak = peakMemory.load();
    while (peak < committed && !peakMemory.compare_exchange_weak(peak, committed))
    {
    }
}

bool Metrics::MemoryAllowsLogin()
{
    auto limit = memoryLimit.load(std::memory_order_relaxed);
    return !limit || privateMemory.load(std::memory_order_relaxed) < limit * 9 / 10;
}

void Metrics::InitialItemGrant(std::uint32_t units)
{
    initialInventoryUnits.fetch_add(units, std::memory_order_relaxed);
}
void Metrics::InitialMoneyGrant(std::uint32_t copper)
{
    initialMoneyCopper.fetch_add(copper, std::memory_order_relaxed);
}
void Metrics::DeniedGrant() { deniedGrants.fetch_add(1, std::memory_order_relaxed); }

void Metrics::Population(std::uint32_t online, std::uint32_t humanZone, std::uint32_t mandatory,
                         std::uint32_t background, std::uint32_t changes, std::uint64_t longestWait)
{
    onlinePopulation.store(online, std::memory_order_relaxed);
    humanZonePopulation.store(humanZone, std::memory_order_relaxed);
    mandatoryPopulation.store(mandatory, std::memory_order_relaxed);
    backgroundPopulation.store(background, std::memory_order_relaxed);
    activityChanges.fetch_add(changes, std::memory_order_relaxed);
    longestBackgroundWait.store(longestWait, std::memory_order_relaxed);
}

void Metrics::WorldTick(std::uint32_t milliseconds)
{
    std::lock_guard lock(samplesMutex);
    samples[sampleCursor++ % samples.size()] = milliseconds;
    sampleCount = std::min(sampleCount + 1, samples.size());
}

void Metrics::Processing(bool world, std::uint64_t microseconds)
{
    auto index = world ? 1 : 0;
    processingCount[index].fetch_add(1, std::memory_order_relaxed);
    processingTime[index].fetch_add(microseconds, std::memory_order_relaxed);
    auto peak = processingPeak[index].load(std::memory_order_relaxed);
    while (peak < microseconds &&
           !processingPeak[index].compare_exchange_weak(peak, microseconds, std::memory_order_relaxed))
    {
    }
}

std::string Metrics::Summary()
{
    std::vector<std::uint32_t> sorted;
    {
        std::lock_guard lock(samplesMutex);
        sorted.assign(samples.begin(), samples.begin() + sampleCount);
    }
    std::sort(sorted.begin(), sorted.end());
    auto quantile = [&](std::size_t percent)
    { return sorted.empty() ? 0 : sorted[(sorted.size() * percent + 99) / 100 - 1]; };
    std::ostringstream out;
    out << " world_samples=" << sorted.size() << " world_p95_ms=" << quantile(95) << " world_p99_ms=" << quantile(99);
    out << " initial_inventory_units=" << initialInventoryUnits.load(std::memory_order_relaxed)
        << " initial_money_copper=" << initialMoneyCopper.load(std::memory_order_relaxed)
        << " rejected_factory_grants=" << deniedGrants.load(std::memory_order_relaxed);
    out << " online=" << onlinePopulation.load(std::memory_order_relaxed)
        << " human_zone=" << humanZonePopulation.load(std::memory_order_relaxed)
        << " mandatory=" << mandatoryPopulation.load(std::memory_order_relaxed)
        << " background=" << backgroundPopulation.load(std::memory_order_relaxed)
        << " activation_changes=" << activityChanges.load(std::memory_order_relaxed)
        << " background_longest_wait_s=" << longestBackgroundWait.load(std::memory_order_relaxed);
    out << " process_memory_limit_bytes=" << memoryLimit.load() << " process_private_bytes=" << privateMemory.load()
        << " process_resident_bytes=" << residentMemory.load() << " process_peak_private_bytes=" << peakMemory.load()
        << " memory_login_blocked=" << !MemoryAllowsLogin();
    for (std::size_t index = 0; index < 2; ++index)
    {
        auto count = processingCount[index].load(std::memory_order_relaxed);
        out << (index ? " roster" : " bot") << "_calls=" << count
            << " mean_us=" << (count ? processingTime[index].load(std::memory_order_relaxed) / count : 0)
            << " peak_us=" << processingPeak[index].load(std::memory_order_relaxed);
    }
    return out.str();
}
}  // namespace Advai
