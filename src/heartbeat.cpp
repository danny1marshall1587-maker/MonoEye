#include "layer.h"
#include "logging.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace monoeye {

extern std::mutex s_session_map_mutex;
extern std::unordered_map<XrSession, SessionState> s_session_map;
extern std::atomic<uint64_t> g_frame_count;

static std::thread s_heartbeat_thread;
static std::atomic<bool> s_stop_heartbeat{false};

void heartbeat_loop() {
    auto last_log_time = std::chrono::steady_clock::now();
    uint64_t last_frame_count = 0;

    while (!s_stop_heartbeat) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count();
        
        uint64_t current_frame_count = g_frame_count.load();
        float fps = (elapsed > 0) ? (float)(current_frame_count - last_frame_count) / elapsed : 0.0f;
        
        size_t session_count = 0;
        {
            std::lock_guard<std::mutex> lock(s_session_map_mutex);
            session_count = s_session_map.size();
        }

        MONOEYE_LOG("[HEARTBEAT] Status: %s | Sessions: %zu | Total Frames: %llu | Avg FPS: %.1f", 
                    (session_count > 0 ? "ACTIVE" : "IDLE"), 
                    session_count, 
                    (unsigned long long)current_frame_count,
                    fps);

        last_frame_count = current_frame_count;
        last_log_time = now;
    }
}

void start_heartbeat() {
    if (!s_heartbeat_thread.joinable()) {
        s_stop_heartbeat = false;
        s_heartbeat_thread = std::thread(heartbeat_loop);
    }
}

void stop_heartbeat() {
    s_stop_heartbeat = true;
    if (s_heartbeat_thread.joinable()) {
        s_heartbeat_thread.join();
    }
}

} // namespace monoeye
