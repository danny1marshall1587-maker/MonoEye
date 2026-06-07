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
#include <cstring>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <vector>

namespace monoeye {

static LogLevel s_log_level = LOG_INFO;
static std::atomic<bool> s_log_initialized{false};
static std::atomic<bool> s_log_running{false};
static FILE* s_log_file = nullptr;
static std::thread s_log_thread;
static std::mutex s_queue_mutex;
static std::condition_variable s_queue_cv;
static std::queue<std::string> s_log_queue;

#ifdef _WIN32
static HANDLE s_pipe_handle = INVALID_HANDLE_VALUE;

static void connect_pipe() {
    if (s_pipe_handle == INVALID_HANDLE_VALUE) {
        s_pipe_handle = CreateFileA("\\\\.\\pipe\\MonoEyeLogs", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (s_pipe_handle != INVALID_HANDLE_VALUE) {
            char exe_path[MAX_PATH] = {0};
            GetModuleFileNameA(NULL, exe_path, MAX_PATH);
            const char* filename = exe_path;
            for (const char* p = exe_path; *p; ++p) {
                if (*p == '/' || *p == '\\') filename = p + 1;
            }
            char msg[512];
            snprintf(msg, sizeof(msg), "[SYSTEM] Connected from game process: %s\r\n", filename);
            DWORD written;
            WriteFile(s_pipe_handle, msg, strlen(msg), &written, NULL);
        }
    }
}
#endif

static void log_worker() {
#ifdef _WIN32
    connect_pipe();
#endif
    while (s_log_running) {
        std::string msg;
        {
            std::unique_lock<std::mutex> lock(s_queue_mutex);
            s_queue_cv.wait(lock, [] { return !s_log_queue.empty() || !s_log_running; });
            
            if (s_log_queue.empty() && !s_log_running) {
                break;
            }
            
            msg = std::move(s_log_queue.front());
            s_log_queue.pop();
        }

        // Output to stdout
        fprintf(stdout, "%s\n", msg.c_str());
        fflush(stdout);

        // Output to file
        if (s_log_file) {
            fprintf(s_log_file, "%s\n", msg.c_str());
            fflush(s_log_file);
        }

#ifdef _WIN32
        // Also output to DebugView on Windows
        OutputDebugStringA((msg + "\n").c_str());

        // Output to Named Pipe
        if (s_pipe_handle == INVALID_HANDLE_VALUE) {
            static auto last_connect_try = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_connect_try).count() >= 2) {
                connect_pipe();
                last_connect_try = now;
            }
        }

        if (s_pipe_handle != INVALID_HANDLE_VALUE) {
            std::string pipe_msg = msg + "\r\n";
            DWORD written;
            if (!WriteFile(s_pipe_handle, pipe_msg.c_str(), (DWORD)pipe_msg.length(), &written, NULL)) {
                CloseHandle(s_pipe_handle);
                s_pipe_handle = INVALID_HANDLE_VALUE;
            }
        }
#endif
    }
}

static void ensure_initialized() {
    if (s_log_initialized.exchange(true)) return;

#ifdef _WIN32
    // Unconditional DebugView breadcrumb - confirms DLL is loaded even if file logging fails.
    // Visible in Sysinternals DebugView with no other setup required.
    OutputDebugStringA("[MonoEye] ensure_initialized() called - DLL is loaded and running.\n");
#endif

    // Always use DEBUG level to capture everything during diagnostics
    s_log_level = LOG_DEBUG;

    const char* level_str = getenv("MONOEYE_LOG_LEVEL");
    if (level_str) {
        switch (level_str[0]) {
            case 'd': case 'D': s_log_level = LOG_DEBUG; break;
            case 'i': case 'I': s_log_level = LOG_INFO;  break;
            case 'w': case 'W': s_log_level = LOG_WARN;  break;
            case 'e': case 'E': s_log_level = LOG_ERROR; break;
            default: s_log_level = LOG_DEBUG; break;
        }
    }

    // Log file is ALWAYS created - MONOEYE_LOG_ENABLED only controls log LEVEL not file creation.
    // Previously this gate was silently swallowing all output when the var was unset or "0".
    {
        char log_path[512] = {0};

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
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", log_path);
            int res = system(mkdir_cmd);
            (void)res;
            strncat(log_path, "/monoeye.log", sizeof(log_path) - strlen(log_path) - 1);
        }
#endif
        if (log_path[0] != '\0') {
            s_log_file = fopen(log_path, "w");
            if (s_log_file) {
                fprintf(s_log_file, "=== MonoEye Debug Log ===\n");
                fprintf(s_log_file, "Build Date: %s %s\n", __DATE__, __TIME__);
#ifdef _WIN32
                char exe_path[MAX_PATH] = {0};
                GetModuleFileNameA(NULL, exe_path, MAX_PATH);
                fprintf(s_log_file, "Host Process: %s\n", exe_path);
#endif
                fprintf(s_log_file, "\n");
                fflush(s_log_file);
            }
#ifdef _WIN32
            else {
                char err_msg[600];
                snprintf(err_msg, sizeof(err_msg), "[MonoEye] FAILED to open log file: %s (errno=%d)\n", log_path, errno);
                OutputDebugStringA(err_msg);
            }
#endif
        }
    }

    s_log_running = true;
    s_log_thread = std::thread(log_worker);
}

void monoeye_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
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

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    struct tm time_info;
#ifdef _WIN32
    localtime_s(&time_info, &in_time_t);
#else
    localtime_r(&in_time_t, &time_info);
#endif

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &time_info);

    const char* filename = file;
    for (const char* p = file; *p; ++p) {
        if (*p == '/' || *p == '\\') filename = p + 1;
    }

    char prefix[256];
    snprintf(prefix, sizeof(prefix), "[%s.%03d][MonoEye][%s] %s:%d: ", 
             time_str, (int)ms.count(), level_str, filename, line);

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    std::string full_msg = prefix;
    full_msg += buf;

    {
        std::lock_guard<std::mutex> lock(s_queue_mutex);
        if (s_log_queue.size() < 1000) { // Safety cap to prevent memory bloat
            s_log_queue.push(std::move(full_msg));
            s_queue_cv.notify_one();
        }
    }
}

LogLevel get_log_level() {
    ensure_initialized();
    return s_log_level;
}

} // namespace monoeye
