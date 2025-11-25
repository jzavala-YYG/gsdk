#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <mutex>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <cstring>

#include "gsdk.h"  // from cppsdk/include

using namespace Microsoft::Azure::Gaming;

static std::mutex   g_mutex;
static std::string  g_lastString;       // last normal return (for debugging only)
static std::string  g_lastErrorString;  // last error message (human readable)

// C-style buffers exposed to GameMaker
static char g_lastReturnBuffer[4096] = { 0 };
static char g_lastErrorBuffer[4096]  = { 0 };

static bool         g_started = false;
static bool         g_healthy = true;

#ifdef _WIN32
    #define GSDKWRAP_API extern "C" __declspec(dllexport)
#else
    #define GSDKWRAP_API extern "C"
#endif

// ----------------- internal helpers -----------------

static char* MakeReturnString(const std::string& s)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    g_lastString = s;

    const size_t maxLen = sizeof(g_lastReturnBuffer) - 1;
    const size_t len    = (s.size() < maxLen) ? s.size() : maxLen;

    std::memcpy(g_lastReturnBuffer, s.c_str(), len);
    g_lastReturnBuffer[len] = '\0';

    return g_lastReturnBuffer;
}

static void SetLastError(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    g_lastErrorString = msg;

    const size_t maxLen = sizeof(g_lastErrorBuffer) - 1;
    const size_t len    = (msg.size() < maxLen) ? msg.size() : maxLen;

    std::memcpy(g_lastErrorBuffer, msg.c_str(), len);
    g_lastErrorBuffer[len] = '\0';

    // Also log to stdout for container logs
    std::cout << "[GSDKWRAP ERROR] " << g_lastErrorBuffer << std::endl;
}

// Helper to get config key, now returning char*
static char* GetConfigKey(const char* key)
{
    if (!key)
    {
        SetLastError("GetConfigKey: key is nullptr");
        return nullptr;
    }

    try
    {
        auto cfg = GSDK::getConfigSettings();
        auto it  = cfg.find(key);
        if (it == cfg.end())
        {
            std::string msg = "GetConfigKey: key not found: ";
            msg += key;
            SetLastError(msg);
            return nullptr;
        }

        return MakeReturnString(it->second);
    }
    catch (const std::exception& ex)
    {
        std::string msg = "Exception in getConfigSettings: ";
        msg += ex.what();
        SetLastError(msg);
        return nullptr;
    }
    catch (...)
    {
        SetLastError("Unknown exception in getConfigSettings");
        return nullptr;
    }
}

// ----------------- exported API -----------------

GSDKWRAP_API double gsdk_start(double debugLogs)
{
	std::cout << "gsdk_start CALLED: " << debugLogs << std::endl;
	
    if (g_started)
    {
        std::cout << "GSDK already started" << std::endl;
        return 1.0;
    }

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
        std::string msg = "GSDKInitializationException: ";
        msg += ex.what();
        SetLastError(msg);
        return -1.0;
    }
    catch (const std::exception& ex)
    {
        std::string msg = "std::exception from GSDK::start: ";
        msg += ex.what();
        SetLastError(msg);
        return -2.0;
    }
    catch (...)
    {
        SetLastError("Unknown exception from GSDK::start");
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
        std::string msg = "Exception in readyForPlayers: ";
        msg += ex.what();
        SetLastError(msg);
        return -1.0;
    }
    catch (...)
    {
        SetLastError("Unknown exception in readyForPlayers");
        return -2.0;
    }
}

// ----------------- string-returning API (now char*) -----------------

GSDKWRAP_API char* gsdk_get_config_value(const char* key)
{
    return GetConfigKey(key);
}

// ----------------- extra helpers: config keys -----------------

GSDKWRAP_API char* gsdk_get_title_id()
{
    return GetConfigKey(GSDK::TITLE_ID_KEY);
}

GSDKWRAP_API char* gsdk_get_build_id()
{
    return GetConfigKey(GSDK::BUILD_ID_KEY);
}

GSDKWRAP_API char* gsdk_get_region()
{
    return GetConfigKey(GSDK::REGION_KEY);
}

GSDKWRAP_API char* gsdk_get_server_id()
{
    return GetConfigKey(GSDK::SERVER_ID_KEY);
}

GSDKWRAP_API char* gsdk_get_vm_id()
{
    return GetConfigKey(GSDK::VM_ID_KEY);
}

GSDKWRAP_API char* gsdk_get_public_ip_v4()
{
    return GetConfigKey(GSDK::PUBLIC_IP_V4_ADDRESS_KEY);
}

GSDKWRAP_API char* gsdk_get_fqdn()
{
    return GetConfigKey(GSDK::FULLY_QUALIFIED_DOMAIN_NAME_KEY);
}

GSDKWRAP_API char* gsdk_get_session_id()
{
    return GetConfigKey(GSDK::SESSION_ID_KEY);
}

GSDKWRAP_API char* gsdk_get_session_cookie()
{
    return GetConfigKey(GSDK::SESSION_COOKIE_KEY);
}

// ----------------- directories & logging -----------------

GSDKWRAP_API char* gsdk_get_logs_directory()
{
    try
    {
        return MakeReturnString(GSDK::getLogsDirectory());
    }
    catch (const std::exception& ex)
    {
        std::string msg = "Exception in getLogsDirectory: ";
        msg += ex.what();
        SetLastError(msg);
        return nullptr;
    }
    catch (...)
    {
        SetLastError("Unknown exception in getLogsDirectory");
        return nullptr;
    }
}

GSDKWRAP_API char* gsdk_get_shared_content_directory()
{
    try
    {
        return MakeReturnString(GSDK::getSharedContentDirectory());
    }
    catch (const std::exception& ex)
    {
        std::string msg = "Exception in getSharedContentDirectory: ";
        msg += ex.what();
        SetLastError(msg);
        return nullptr;
    }
    catch (...)
    {
        SetLastError("Unknown exception in getSharedContentDirectory");
        return nullptr;
    }
}

// return the GSDK logMessage ID as double
GSDKWRAP_API double gsdk_log_message(const char* msg)
{
    if (!msg)
    {
        SetLastError("logMessage: msg is nullptr");
        return -1.0;
    }

    try
    {
        unsigned int id = GSDK::logMessage(std::string(msg));
        return static_cast<double>(id);
    }
    catch (const std::exception& ex)
    {
        std::string err = "Exception in logMessage: ";
        err += ex.what();
        SetLastError(err);
        return -1.0;
    }
    catch (...)
    {
        SetLastError("Unknown exception in logMessage");
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
        std::string msg = "Exception in getInitialPlayers: ";
        msg += ex.what();
        SetLastError(msg);
        return -1.0;
    }
    catch (...)
    {
        SetLastError("Unknown exception in getInitialPlayers");
        return -1.0;
    }
}

// Get player ID by index [0..count-1]
GSDKWRAP_API char* gsdk_get_initial_player(double index)
{
    try
    {
        const std::vector<std::string>& players = GSDK::getInitialPlayers();

        if (index < 0.0)
        {
            SetLastError("getInitialPlayer: index < 0");
            return nullptr;
        }

        size_t i = static_cast<size_t>(index);
        if (i >= players.size())
        {
            SetLastError("getInitialPlayer: index out of range");
            return nullptr;
        }

        return MakeReturnString(players[i]);
    }
    catch (const std::exception& ex)
    {
        std::string msg = "Exception in getInitialPlayers(index): ";
        msg += ex.what();
        SetLastError(msg);
        return nullptr;
    }
    catch (...)
    {
        SetLastError("Unknown exception in getInitialPlayers(index)");
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
        std::string msg = "Exception in gsdk_update_connected_players: ";
        msg += ex.what();
        SetLastError(msg);
        return -1.0;
    }
    catch (...)
    {
        SetLastError("Unknown exception in gsdk_update_connected_players");
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
        std::string msg = "Exception in gsdk_end_session: ";
        msg += ex.what();
        SetLastError(msg);
        return -1.0;
    }
    catch (...)
    {
        SetLastError("Unknown exception in gsdk_end_session");
        return -2.0;
    }
}

// ----------------- last error access -----------------

// Returns the last error message as a char* from a global buffer.
// - nullptr if no error has been recorded yet.
// - Value is valid until the next error, like the other string returns.
GSDKWRAP_API char* gsdk_get_last_error()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_lastErrorBuffer[0] == '\0')
        return nullptr;

    return g_lastErrorBuffer;
}
