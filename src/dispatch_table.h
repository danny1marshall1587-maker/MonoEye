// Copyright (c) 2026 MonoEye Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <openxr/openxr.h>

#include <unordered_map>
#include <mutex>

// Include the generated dispatch table definition
#include "xr_generated_dispatch_table.h"

namespace monoeye {

// Global dispatch table map: instance -> dispatch table
extern std::unordered_map<XrInstance, XrGeneratedDispatchTable*> g_instance_dispatch_map;
extern std::mutex g_instance_dispatch_mutex;

// Generated function that populates the dispatch table
void GeneratedXrPopulateDispatchTable(
    XrGeneratedDispatchTable* dispatch_table,
    XrInstance instance,
    PFN_xrGetInstanceProcAddr get_instance_proc_addr
);

// Clean up dispatch table when instance is destroyed
void MonoEyeCleanUpDispatchTable(XrGeneratedDispatchTable* dispatch_table);

} // namespace monoeye
