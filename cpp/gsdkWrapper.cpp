#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <atomic>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <deque>
#include <ctime>
#include <cstdio>

#include "gsdk.h"  // from cppsdk/include

using namespace Microsoft::Azure::Gaming;

static std::mutex   g_mutex;
static std::string  g_lastString;
static std::string  g_lastErrorString;

// return buffers exposed to GameMaker
static char g_lastReturnBuffer[4096] = { 0 };
static char g_lastErrorBuffer[4096]  = { 0 };

// lifecycle flags
static std::atomic<bool> g_started{ false };
static std::atomic<bool> g_healthy{ true };
static std::atomic<bool> g_shutdownRequested{ false };

// event queue
static std::mutex g_eventsMutex;
static std::deque<std::string> g_events;
static const size_t GSDK_MAX_EVENTS = 256;

#ifdef _WIN32
    #define GSDKWRAP_API extern "C" __declspec(dllexport)
#else
    #define GSDKWRAP_API extern "C"
#endif

// ----------------------------------------------------
// internal helpers
// ----------------------------------------------------

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
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        g_lastErrorString = msg;

        const size_t maxLen = sizeof(g_lastErrorBuffer) - 1;
        const size_t len    = (msg.size() < maxLen) ? msg.size() : maxLen;

        std::memcpy(g_lastErrorBuffer, msg.c_str(), len);
        g_lastErrorBuffer[len] = '\0';
    }

    std::cout << "[GSDKWRAP ERROR] " << msg << std::endl;

    // also push as event
    std::lock_guard<std::mutex> lk(g_eventsMutex);
    if (g_events.size() >= GSDK_MAX_EVENTS)
        g_events.pop_front();

    std::string ev = "{\"type\":\"error\",\"message\":\"";
    for (char c : msg)
    {
        switch (c)
        {
            case '\\': ev += "\\\\"; break;
            case '"':  ev += "\\\""; break;
            case '\n': ev += "\\n";  break;
            case '\r': ev += "\\r";  break;
            case '\t': ev += "\\t";  break;
            default:   ev += c;      break;
        }
    }
    ev += "\"}";
    g_events.push_back(ev);
}

static void PushEvent(const std::string& json)
{
    std::lock_guard<std::mutex> lk(g_eventsMutex);

    if (g_events.size() >= GSDK_MAX_EVENTS)
        g_events.pop_front();

    g_events.push_back(json);
}

static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 16);

    for (char c : s)
    {
        switch (c)
        {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }

    return out;
}

static std::string FormatTmJson(const tm& t)
{
    char buf[64];
    std::snprintf(
        buf,
        sizeof(buf),
        "%04d-%02d-%02d %02d:%02d:%02d",
        t.tm_year + 1900,
        t.tm_mon + 1,
        t.tm_mday,
        t.tm_hour,
        t.tm_min,
        t.tm_sec
    );
    return std::string(buf);
}

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

// ----------------------------------------------------
// exported API
// ----------------------------------------------------
GSDKWRAP_API double gsdk_test()
{
	return 1993;
}

GSDKWRAP_API double gsdk_start(double debugLogs)
{
    std::cout << "gsdk_start CALLED: " << debugLogs << std::endl;

    if (g_started.load())
    {
        std::cout << "GSDK already started" << std::endl;
        return 1.0;
    }

    try
    {
        g_healthy.store(true);
        g_shutdownRequested.store(false);

        GSDK::registerHealthCallback([]() -> bool {
            return g_healthy.load();
        });

        GSDK::registerShutdownCallback([]() {
            std::cout << "GSDK shutdown callback" << std::endl;
            g_shutdownRequested.store(true);
            g_healthy.store(false);
            PushEvent("{\"type\":\"shutdown\"}");
        });

        GSDK::registerMaintenanceCallback([](const tm& when) {
            std::cout << "GSDK maintenance callback" << std::endl;

            std::string timeStr = FormatTmJson(when);
            std::string ev = "{\"type\":\"maintenance\",\"scheduled\":\"" + JsonEscape(timeStr) + "\"}";
            PushEvent(ev);
        });

        GSDK::start(debugLogs != 0);

        g_started.store(true);
        PushEvent("{\"type\":\"started\"}");

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
    std::cout << "gsdk_ready_for_players CALLED" << std::endl;

    try
    {
        bool allocated = GSDK::readyForPlayers();
        std::cout << "readyForPlayers returned " << allocated << std::endl;

        if (allocated)
            PushEvent("{\"type\":\"ready\",\"allocated\":true}");
        else
            PushEvent("{\"type\":\"ready\",\"allocated\":false}");

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

// ----------------------------------------------------
// event polling
// ----------------------------------------------------

GSDKWRAP_API double gsdk_event_count()
{
    std::lock_guard<std::mutex> lk(g_eventsMutex);
    return static_cast<double>(g_events.size());
}

GSDKWRAP_API char* gsdk_poll_event()
{
    std::lock_guard<std::mutex> lk(g_eventsMutex);

    if (g_events.empty())
        return nullptr;

    std::string ev = g_events.front();
    g_events.pop_front();

    return MakeReturnString(ev);
}

GSDKWRAP_API double gsdk_clear_events()
{
    std::lock_guard<std::mutex> lk(g_eventsMutex);
    double count = static_cast<double>(g_events.size());
    g_events.clear();
    return count;
}

// ----------------------------------------------------
// state helpers
// ----------------------------------------------------

GSDKWRAP_API double gsdk_is_started()
{
    return g_started.load() ? 1.0 : 0.0;
}

GSDKWRAP_API double gsdk_is_healthy()
{
    return g_healthy.load() ? 1.0 : 0.0;
}

GSDKWRAP_API double gsdk_has_pending_shutdown()
{
    return g_shutdownRequested.load() ? 1.0 : 0.0;
}

GSDKWRAP_API double gsdk_set_healthy(double healthy)
{
    g_healthy.store(healthy != 0.0);
    return g_healthy.load() ? 1.0 : 0.0;
}

// ----------------------------------------------------
// string-returning API
// ----------------------------------------------------

GSDKWRAP_API char* gsdk_get_config_value(const char* key)
{
    return GetConfigKey(key);
}

// ----------------------------------------------------
// extra helpers: config keys
// ----------------------------------------------------

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

// ----------------------------------------------------
// directories & logging
// ----------------------------------------------------

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

// ----------------------------------------------------
// initial players
// ----------------------------------------------------

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

// ----------------------------------------------------
// update connected players
// ----------------------------------------------------

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
                size_t start = token.find_first_not_of(" \t\r\n");
                size_t end   = token.find_last_not_of(" \t\r\n");
                if (start == std::string::npos || end == std::string::npos)
                    continue;

                std::string id = token.substr(start, end - start + 1);
                if (!id.empty())
                    players.emplace_back(id);
            }
        }

        GSDK::updateConnectedPlayers(players);

        std::cout << "gsdk_update_connected_players: count=" << players.size() << std::endl;

        {
            std::ostringstream oss;
            oss << "{\"type\":\"connected_players_updated\",\"count\":" << players.size() << "}";
            PushEvent(oss.str());
        }

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

        std::vector<ConnectedPlayer> emptyList;
        GSDK::updateConnectedPlayers(emptyList);

        g_healthy.store(false);
        g_shutdownRequested.store(true);

        PushEvent("{\"type\":\"session_ended\"}");

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

// ----------------------------------------------------
// last error access
// ----------------------------------------------------

GSDKWRAP_API char* gsdk_get_last_error()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_lastErrorBuffer[0] == '\0')
        return nullptr;

    return g_lastErrorBuffer;
}

GSDKWRAP_API double gsdk_clear_last_error()
{
    std::lock_guard<std::mutex> lock(g_mutex);

    g_lastErrorString.clear();
    g_lastErrorBuffer[0] = '\0';

    return 1.0;
}