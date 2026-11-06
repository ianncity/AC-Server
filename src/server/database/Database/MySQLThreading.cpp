/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "MySQLThreading.h"
#include "MySQLWorkaround.h"
#include <atomic>
#include <cstdio>
#include <stdexcept>

namespace
{
std::atomic<bool> libraryReady{false};

struct ClientThread
{
    bool initialized = false;

    ~ClientThread()
    {
        if (initialized && libraryReady.load(std::memory_order_acquire))
        {
            std::fprintf(stderr, "[mysql-thread] end begin state=%p\n", static_cast<void*>(this));
            mysql_thread_end();
            std::fprintf(stderr, "[mysql-thread] end complete state=%p\n", static_cast<void*>(this));
        }
    }
};

ClientThread& GetClientThread()
{
    thread_local ClientThread state;
    return state;
}
}

void MySQL::Library_Init()
{
    std::fprintf(stderr, "[mysql-thread] library init begin\n");
    if (mysql_library_init(-1, nullptr, nullptr))
        throw std::runtime_error("Could not initialize the MySQL client library");
    libraryReady.store(true, std::memory_order_release);
    // mysql_library_init() initializes MySQL thread state for its calling thread.
    GetClientThread().initialized = true;
    std::fprintf(stderr, "[mysql-thread] library init complete state=%p\n", static_cast<void*>(&GetClientThread()));
}

void MySQL::Library_End()
{
    ClientThread& state = GetClientThread();
    if (state.initialized)
    {
        std::fprintf(stderr, "[mysql-thread] library end thread end begin state=%p\n", static_cast<void*>(&state));
        mysql_thread_end();
        state.initialized = false;
    }
    std::fprintf(stderr, "[mysql-thread] library end begin\n");
    mysql_library_end();
    libraryReady.store(false, std::memory_order_release);
    std::fprintf(stderr, "[mysql-thread] library end complete\n");
}

void MySQL::EnsureThreadInitialized()
{
    if (!libraryReady.load(std::memory_order_acquire))
        throw std::logic_error("MySQL client used outside its library lifetime");

    ClientThread& state = GetClientThread();
    if (!state.initialized)
    {
        std::fprintf(stderr, "[mysql-thread] init begin state=%p\n", static_cast<void*>(&state));
        if (mysql_thread_init())
            throw std::runtime_error("Could not initialize MySQL client thread state");
        state.initialized = true;
        std::fprintf(stderr, "[mysql-thread] init complete state=%p\n", static_cast<void*>(&state));
    }
}

uint32 MySQL::GetLibraryVersion()
{
    return MYSQL_VERSION_ID;
}
