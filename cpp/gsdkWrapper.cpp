#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <mutex>
#include <string>
#include <vector>
#include <iostream>

#include "gsdk.h"  // from cppsdk/include

using namespace Microsoft::Azure::Gaming;

static std::mutex   g_mutex;
static std::string  g_lastString;
static bool         g_started = false;
static bool         g_healthy = true;

#ifdef _WIN32
    #define GSDKWRAP_API extern "C" __declspec(dllexport)
#else
    #define GSDKWRAP_API extern "C"
#endif

// ----------------- internal helpers -----------------

static const char* MakeReturnString(const std::string& s)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_lastString = s;
    return g_lastString.c_str();
}

static const char* GetConfigKey(const char* key)
{
    if (!key) return nullptr;

    try
    {
        auto cfg = GSDK::getConfigSettings();
        auto it  = cfg.find(key);
        if (it == cfg.end())
            return nullptr;

        return MakeReturnString(it->second);
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in getConfigSettings: " << ex.what() << std::endl;
        return nullptr;
    }
    catch (...)
    {
        std::cout << "Unknown exception in getConfigSettings" << std::endl;
        return nullptr;
    }
}

// ----------------- exported API -----------------

GSDKWRAP_API double gsdk_start(double debugLogs)
{
    if (g_started)
    {
        std::cout << "GSDK already started" << std::endl;
        return 1.0;
    }

    std::cout << "gsdk_start CALLED" << std::endl;

    try
    {
        GSDK::registerHealthCallback([]() -> bool {
			std::cout << "GSDK health callback" << std::endl;
            return g_healthy;
        });

        GSDK::registerShutdownCallback([]() {
            std::cout << "GSDK shutdown callback" << std::endl;
            g_healthy = false;
        });

        GSDK::registerMaintenanceCallback([](const tm&) {
            std::cout << "GSDK maintenance callback" << std::endl;
        });

        GSDK::start(debugLogs != 0);

        g_started = true;
        std::cout << "GSDK::start OK" << std::endl;
        return 1.0;
    }
    catch (const GSDKInitializationException& ex)
    {
        std::cout << "GSDKInitializationException: " << ex.what() << std::endl;
        return -1.0;
    }
    catch (const std::exception& ex)
    {
        std::cout << "std::exception from GSDK::start: " << ex.what() << std::endl;
        return -2.0;
    }
    catch (...)
    {
        std::cout << "Unknown exception from GSDK::start" << std::endl;
        return -3.0;
    }
}

GSDKWRAP_API double gsdk_ready_for_players()
{
    std::cout << "gsdk_ready CALLED" << std::endl;

    try
    {
        bool allocated = GSDK::readyForPlayers();
        std::cout << "readyForPlayers returned " << allocated << std::endl;
        return allocated ? 1.0 : 0.0;
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in readyForPlayers: " << ex.what() << std::endl;
        return -1.0;
    }
    catch (...)
    {
        std::cout << "Unknown exception in readyForPlayers"<< std::endl;
        return -2.0;
    }
}

GSDKWRAP_API const char* gsdk_get_config_value(const char* key)
{
    return GetConfigKey(key);
}

// ----------------- extra helpers: config keys -----------------

GSDKWRAP_API const char* gsdk_get_title_id()
{
    return GetConfigKey(GSDK::TITLE_ID_KEY);
}

GSDKWRAP_API const char* gsdk_get_build_id()
{
    return GetConfigKey(GSDK::BUILD_ID_KEY);
}

GSDKWRAP_API const char* gsdk_get_region()
{
    return GetConfigKey(GSDK::REGION_KEY);
}

GSDKWRAP_API const char* gsdk_get_server_id()
{
    return GetConfigKey(GSDK::SERVER_ID_KEY);
}

GSDKWRAP_API const char* gsdk_get_vm_id()
{
    return GetConfigKey(GSDK::VM_ID_KEY);
}

GSDKWRAP_API const char* gsdk_get_public_ip_v4()
{
    return GetConfigKey(GSDK::PUBLIC_IP_V4_ADDRESS_KEY);
}

GSDKWRAP_API const char* gsdk_get_fqdn()
{
    return GetConfigKey(GSDK::FULLY_QUALIFIED_DOMAIN_NAME_KEY);
}

GSDKWRAP_API const char* gsdk_get_session_id()
{
    return GetConfigKey(GSDK::SESSION_ID_KEY);
}

GSDKWRAP_API const char* gsdk_get_session_cookie()
{
    return GetConfigKey(GSDK::SESSION_COOKIE_KEY);
}

// ----------------- directories & logging -----------------

GSDKWRAP_API const char* gsdk_get_logs_directory()
{
    try
    {
        return MakeReturnString(GSDK::getLogsDirectory());
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in getLogsDirectory: " << ex.what() << std::endl;
        return nullptr;
    }
    catch (...)
    {
        std::cout << "Unknown exception in getLogsDirectory" << std::endl;
        return nullptr;
    }
}

GSDKWRAP_API const char* gsdk_get_shared_content_directory()
{
    try
    {
        return MakeReturnString(GSDK::getSharedContentDirectory());
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in getSharedContentDirectory: " << ex.what() << std::endl;
        return nullptr;
    }
    catch (...)
    {
        std::cout << "Unknown exception in getSharedContentDirectory" << std::endl;
        return nullptr;
    }
}

// return the GSDK logMessage ID as double
GSDKWRAP_API double gsdk_log_message(const char* msg)
{
    if (!msg) return -1.0;

    try
    {
        unsigned int id = GSDK::logMessage(std::string(msg));
        return static_cast<double>(id);
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in logMessage: " << ex.what() << std::endl;
        return -1.0;
    }
    catch (...)
    {
        std::cout << "Unknown exception in logMessage" << std::endl;
        return -1.0;
    }
}

// ----------------- initial players -----------------

// How many initial players are assigned (after allocation)
GSDKWRAP_API double gsdk_get_initial_player_count()
{
    try
    {
        const std::vector<std::string>& players = GSDK::getInitialPlayers();
        return static_cast<double>(players.size());
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in getInitialPlayers: " << ex.what() << std::endl;
        return -1.0;
    }
    catch (...)
    {
        std::cout << "Unknown exception in getInitialPlayers" << std::endl;
        return -1.0;
    }
}

// Get player ID by index [0..count-1]
GSDKWRAP_API const char* gsdk_get_initial_player(double index)
{
    try
    {
        const std::vector<std::string>& players = GSDK::getInitialPlayers();

        if (index < 0.0)
            return nullptr;

        size_t i = static_cast<size_t>(index);
        if (i >= players.size())
            return nullptr;

        return MakeReturnString(players[i]);
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in getInitialPlayers(index): " << ex.what() << std::endl;
        return nullptr;
    }
    catch (...)
    {
        std::cout << "Unknown exception in getInitialPlayers(index)" << std::endl;
        return nullptr;
    }
}


// ----------------- update connected players -----------------
//
// idsCsv:
//   - nullptr or ""  -> clears all connected players
//   - "p1"           -> one player
//   - "p1,p2,p3"     -> multiple players
//
GSDKWRAP_API double gsdk_update_connected_players(const char* idsCsv)
{
    try
    {
        std::vector<ConnectedPlayer> players;

        if (idsCsv && idsCsv[0] != '\0')
        {
            std::string all(idsCsv);
            std::stringstream ss(all);
            std::string token;

            while (std::getline(ss, token, ','))
            {
                // remove simple spaces around id
                size_t start = token.find_first_not_of(" \t\r\n");
                size_t end   = token.find_last_not_of(" \t\r\n");
                if (start == std::string::npos || end == std::string::npos)
                    continue;

                std::string id = token.substr(start, end - start + 1);
                if (!id.empty())
                {
                    players.emplace_back(id); // ConnectedPlayer(std::string)
                }
            }
        }

        GSDK::updateConnectedPlayers(players);
        std::cout << "gsdk_update_connected_players: count=" << players.size() << std::endl;

        return static_cast<double>(players.size());
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in gsdk_update_connected_players: " << ex.what() << std::endl;
        return -1.0;
    }
    catch (...)
    {
        std::cout << "Unknown exception in gsdk_update_connected_players" << std::endl;
        return -2.0;
    }
}



GSDKWRAP_API double gsdk_end_session()
{
    try
    {
        std::cout << "gsdk_end_session CALLED" << std::endl;

        // Clear connected players
        std::vector<ConnectedPlayer> emptyList;
        GSDK::updateConnectedPlayers(emptyList);

        // Mark unhealthy so health callback starts failing
        g_healthy = false;

        std::cout << "gsdk_end_session: cleared players and marked unhealthy" << std::endl;
        return 1.0;
    }
    catch (const std::exception& ex)
    {
        std::cout << "Exception in gsdk_end_session: " << ex.what() << std::endl;
        return -1.0;
    }
    catch (...)
    {
        std::cout << "Unknown exception in gsdk_end_session" << std::endl;
        return -2.0;
    }
}

