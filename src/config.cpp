// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "config.h"
#include "logging.h"

#include <cstdlib>
#include <cstring>

namespace monoeye {

std::string get_env(const std::string& name) {
    const char* value = getenv(name.c_str());
    return value ? std::string(value) : "";
}

std::optional<std::string> get_env_opt(const std::string& name) {
    const char* value = getenv(name.c_str());
    return value ? std::optional<std::string>(value) : std::nullopt;
}

bool get_env_bool(const std::string& name, bool default_value) {
    const char* value = getenv(name.c_str());
    if (!value) return default_value;

    std::string v(value);
    if (v == "1" || v == "true" || v == "True" || v == "TRUE" || v == "yes" || v == "on") {
        return true;
    }
    if (v == "0" || v == "false" || v == "False" || v == "FALSE" || v == "no" || v == "off") {
        return false;
    }
    return default_value;
}

float get_env_float(const std::string& name, float default_value) {
    const char* value = getenv(name.c_str());
    if (!value) return default_value;
    try {
        return std::stof(value);
    } catch (...) {
        return default_value;
    }
}

int get_env_int(const std::string& name, int default_value) {
    const char* value = getenv(name.c_str());
    if (!value) return default_value;
    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

Config load_config() {
    Config config;

    config.enabled = get_env_bool("MONOEYE_ENABLE", true);
    config.bypass_mode = get_env_bool("MONOEYE_BYPASS", false);
    config.debug_mode = get_env_bool("MONOEYE_DEBUG_MODE", false);
    config.ipd_override = get_env_float("MONOEYE_IPD_OVERRIDE", 0.0f);
    config.render_width_percent = get_env_float("MONOEYE_RENDER_WIDTH_PERCENT", 100.0f);
    config.warp_quality = get_env_int("MONOEYE_WARP_QUALITY", 1);
    config.show_indicator = get_env_bool("MONOEYE_INDICATOR", true);

    auto debug_path = get_env_opt("MONOEYE_DEBUG_OUTPUT_PATH");
    if (debug_path) {
        config.debug_output_path = *debug_path;
    }

    auto log_path = get_env_opt("MONOEYE_LOG_FILE");
    if (log_path) {
        config.log_file_path = *log_path;
    }

    MONOEYE_LOG("Configuration loaded:");
    MONOEYE_LOG("  enabled: %s", config.enabled ? "true" : "false");
    MONOEYE_LOG("  bypass_mode: %s", config.bypass_mode ? "true" : "false");
    MONOEYE_LOG("  debug_mode: %s", config.debug_mode ? "true" : "false");
    MONOEYE_LOG("  ipd_override: %.4f", config.ipd_override);
    MONOEYE_LOG("  render_width_percent: %.1f", config.render_width_percent);
    MONOEYE_LOG("  warp_quality: %d", config.warp_quality);
    MONOEYE_LOG("  show_indicator: %s", config.show_indicator ? "true" : "false");

    return config;
}

// Global config instance
static Config s_config;

const Config& get_config() {
    return s_config;
}

} // namespace monoeye
