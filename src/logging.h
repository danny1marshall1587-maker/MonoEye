// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdio>
#include <cstdarg>

namespace monoeye {

// Logging levels
enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

void monoeye_log(LogLevel level, const char* file, int line, const char* fmt, ...);

// Get current log level (controlled by MONOEYE_LOG_LEVEL env var)
LogLevel get_log_level();

// Check if a log level is enabled
inline bool log_enabled(LogLevel level) {
    return level >= get_log_level();
}

} // namespace monoeye

// Logging macros with file/line info
#define MONOEYE_LOG(fmt, ...) \
    monoeye::monoeye_log(monoeye::LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define MONOEYE_LOG_DEBUG(fmt, ...) \
    monoeye::monoeye_log(monoeye::LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define MONOEYE_LOG_WARN(fmt, ...) \
    monoeye::monoeye_log(monoeye::LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define MONOEYE_LOG_ERROR(fmt, ...) \
    monoeye::monoeye_log(monoeye::LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
