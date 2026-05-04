// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <optional>

namespace monoeye {

struct Config {
    // Enable/disable the depth warp
    bool enabled = true;

    // Enable/disable passthrough mode (no warp, just forward frames)
    bool bypass_mode = false;

    // Enable debug mode (screenshots, timing output)
    bool debug_mode = false;

    // Show a small green dot in the headset to indicate MonoEye is active
    bool show_indicator = true;

    // IPD override in meters (0.0 = use headset default)
    float ipd_override = 0.0f;

    // Render width percentage for wider frame (future: requires engine support)
    float render_width_percent = 100.0f;

    // Depth warp quality mode: 0=fast, 1=balanced, 2=quality
    int warp_quality = 1;

    // Debug output path for screenshots
    std::string debug_output_path;

    // Log file path (empty = stdout only)
    std::string log_file_path;
};

// Load configuration from environment variables and optional config file
Config load_config();

// Get the current global configuration
const Config& get_config();

// Get a specific environment variable
std::string get_env(const std::string& name);

// Get an optional environment variable (returns nullopt if not set)
std::optional<std::string> get_env_opt(const std::string& name);

// Get environment variable as bool
bool get_env_bool(const std::string& name, bool default_value);

// Get environment variable as float
float get_env_float(const std::string& name, float default_value);

// Get environment variable as int
int get_env_int(const std::string& name, int default_value);

} // namespace monoeye
