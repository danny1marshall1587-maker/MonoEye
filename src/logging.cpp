// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "logging.h"
#include "config.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <mutex>

#include <chrono>
#include <iomanip>
#include <fstream>

namespace monoeye {

static std::mutex s_log_mutex;
static LogLevel s_log_level = LOG_INFO;
static bool s_log_initialized = false;
static FILE* s_log_file = nullptr;

static void ensure_initialized() {
    if (s_log_initialized) return;

    // Default level
    s_log_level = LOG_INFO;

    const char* level_str = getenv("MONOEYE_LOG_LEVEL");
    if (level_str) {
        switch (level_str[0]) {
            case 'd': case 'D': s_log_level = LOG_DEBUG; break;
            case 'i': case 'I': s_log_level = LOG_INFO;  break;
            case 'w': case 'W': s_log_level = LOG_WARN;  break;
            case 'e': case 'E': s_log_level = LOG_ERROR; break;
            default: s_log_level = LOG_DEBUG; break;
        }
    } else {
        // If not specified, but enabled, default to DEBUG
        const char* enabled_str = getenv("MONOEYE_LOG_ENABLED");
        if (enabled_str && (enabled_str[0] == '1' || enabled_str[0] == 't' || enabled_str[0] == 'T')) {
            s_log_level = LOG_DEBUG;
        }
    }

    const char* enabled_str = getenv("MONOEYE_LOG_ENABLED");
    bool logging_enabled = enabled_str && (enabled_str[0] == '1' || enabled_str[0] == 't' || enabled_str[0] == 'T');

    if (logging_enabled) {
        char log_path[512];
        log_path[0] = '\0';

#ifdef _WIN32
        const char* user_profile = getenv("USERPROFILE");
        if (user_profile) {
            snprintf(log_path, sizeof(log_path), "%s\\Documents\\MonoEye", user_profile);
            CreateDirectoryA(log_path, NULL);
            strncat(log_path, "\\monoeye.log", sizeof(log_path) - strlen(log_path) - 1);
        }
#else
        const char* home = getenv("HOME");
        if (home) {
            snprintf(log_path, sizeof(log_path), "%s/Documents/MonoEye", home);
            char mkdir_cmd[512];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", log_path);
            system(mkdir_cmd);
            strncat(log_path, "/monoeye.log", sizeof(log_path) - strlen(log_path) - 1);
        }
#endif
        if (log_path[0] != '\0') {
            s_log_file = fopen(log_path, "w"); // Overwrite mode
            if (s_log_file) {
                fprintf(s_log_file, "=== MonoEye Debug Log ===\n");
                fprintf(s_log_file, "Build Date: %s %s\n", __DATE__, __TIME__);
                fprintf(s_log_file, "=========================\n\n");
            }
        }
    }

    s_log_initialized = true;
}

void monoeye_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(s_log_mutex);
    ensure_initialized();

    if (level < s_log_level) {
        return;
    }

    const char* level_str;
    switch (level) {
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO:  level_str = "INFO "; break;
        case LOG_WARN:  level_str = "WARN "; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        default: level_str = "?????"; break;
    }

    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    char time_str[64];
    // use localtime_r/s if available, but for simplicity:
    struct tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &in_time_t);
#else
    localtime_r(&in_time_t, &time_info);
#endif
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_info);

    // Extract just the filename
    const char* filename = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }

    // Format the prefix
    char prefix[256];
    snprintf(prefix, sizeof(prefix), "[%s.%03d][MonoEye][%s] %s:%d: ", 
             time_str, (int)ms.count(), level_str, filename, line);

    // Output to stdout
    fprintf(stdout, "%s", prefix);
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);

    // Output to file
    if (s_log_file) {
        fprintf(s_log_file, "%s", prefix);
        va_list args_file;
        va_start(args_file, fmt);
        vfprintf(s_log_file, fmt, args_file);
        va_end(args_file);
        fprintf(s_log_file, "\n");
        fflush(s_log_file);
    }

#ifdef _WIN32
    // Also output to DebugView on Windows
    char buf[4096];
    va_list args_win;
    va_start(args_win, fmt);
    snprintf(buf, sizeof(buf), "%s", prefix);
    vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), fmt, args_win);
    va_end(args_win);
    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    OutputDebugStringA(buf);
#endif
}


LogLevel get_log_level() {
    ensure_initialized();
    return s_log_level;
}

} // namespace monoeye


} // namespace monoeye
