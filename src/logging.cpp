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

namespace monoeye {

static std::mutex s_log_mutex;
static LogLevel s_log_level = LOG_INFO;
static bool s_log_initialized = false;

void monoeye_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (!log_enabled(level)) {
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

    std::lock_guard<std::mutex> lock(s_log_mutex);

    // Extract just the filename
    const char* filename = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }

    fprintf(stdout, "[MonoEye][%s] %s:%d: ", level_str, filename, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);

    fprintf(stdout, "\n");
    fflush(stdout);

#ifdef _WIN32
    // Also output to DebugView on Windows
    char buf[4096];
    va_list args2;
    va_start(args2, fmt);
    snprintf(buf, sizeof(buf), "[MonoEye][%s] %s:%d: ", level_str, filename, line);
    vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), fmt, args2);
    va_end(args2);
    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    OutputDebugStringA(buf);
#endif
}

LogLevel get_log_level() {
    if (!s_log_initialized) {
        const char* level_str = getenv("MONOEYE_LOG_LEVEL");
        if (level_str) {
            switch (level_str[0]) {
                case 'd': case 'D': s_log_level = LOG_DEBUG; break;
                case 'i': case 'I': s_log_level = LOG_INFO;  break;
                case 'w': case 'W': s_log_level = LOG_WARN;  break;
                case 'e': case 'E': s_log_level = LOG_ERROR; break;
                default: s_log_level = LOG_INFO; break;
            }
        }
        s_log_initialized = true;
    }
    return s_log_level;
}

} // namespace monoeye
