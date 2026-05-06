// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#include "dispatch_table.h"

#include <unordered_map>
#include <mutex>

namespace monoeye {

std::unordered_map<XrInstance, XrGeneratedDispatchTable*> g_instance_dispatch_map;
std::mutex g_instance_dispatch_mutex;

std::unordered_map<XrSession, SessionState> s_session_map;
std::mutex s_session_map_mutex;

} // namespace monoeye
