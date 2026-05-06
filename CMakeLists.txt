cmake_minimum_required(VERSION 3.18)
project(MonoEye
    VERSION 0.5.33

    DESCRIPTION "MonoEye - Single-eye VR rendering with depth-based second eye reconstruction"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Platform detection
if(WIN32)
    set(PLATFORM_NAME "windows")
    set(LAYER_SUFFIX ".dll")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_NAME "linux")
    set(LAYER_SUFFIX ".so")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Build options
option(MONOEYE_BUILD_TESTS "Build test suite" OFF)
option(MONOEYE_EMBED_SHADERS "Embed shaders as C++ arrays" ON)
option(MONOEYE_STRICT_WARNINGS "Treat warnings as errors" OFF)

# Find required packages
find_package(Vulkan REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Threads REQUIRED)


# OpenXR headers - use submodule if available, otherwise find system
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/OpenXR-SDK/include")
    add_library(openxr_headers INTERFACE)
    target_include_directories(openxr_headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/external/OpenXR-SDK/include"
    )
elseif(DEFINED ENV{OPENXR_SDK_PATH})
    add_library(openxr_headers INTERFACE)
    target_include_directories(openxr_headers INTERFACE "$ENV{OPENXR_SDK_PATH}/include")
else()
    find_path(OPENXR_INCLUDE_DIR NAMES openxr/openxr.h)
    if(NOT OPENXR_INCLUDE_DIR)
        message(FATAL_ERROR "OpenXR headers not found. Clone the OpenXR-SDK submodule or set OPENXR_SDK_PATH.")
    endif()
    add_library(openxr_headers INTERFACE)
    target_include_directories(openxr_headers INTERFACE ${OPENXR_INCLUDE_DIR})
endif()

# GLSL to SPIR-V compilation
if(MONOEYE_EMBED_SHADERS)
    # Search paths for shader compilers
    set(SHADER_COMPILER_HINTS "")
    if(DEFINED ENV{VULKAN_SDK})
        list(APPEND SHADER_COMPILER_HINTS "$ENV{VULKAN_SDK}/Bin")
    endif()

    find_program(GLSLLANG_EXECUTABLE NAMES glslangValidator HINTS ${SHADER_COMPILER_HINTS})
    find_program(GLSLC_EXECUTABLE NAMES glslc HINTS ${SHADER_COMPILER_HINTS})

    if(GLSLC_EXECUTABLE)
        set(SHADER_COMPILER ${GLSLC_EXECUTABLE})
        set(SHADER_COMPILER_NAME "glslc")
    elseif(GLSLLANG_EXECUTABLE)
        set(SHADER_COMPILER ${GLSLLANG_EXECUTABLE})
        set(SHADER_COMPILER_NAME "glslangValidator")
    else()
        message(FATAL_ERROR "Neither glslc nor glslangValidator found. Ensure Vulkan SDK or glslang-tools is installed.")
    endif()

    file(GLOB SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp.glsl")
    set(SHADER_HEADERS "")

    foreach(SHADER_SRC ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME_WE)
        set(SPIRV_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.comp.spv")
        set(HEADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.h")

        if(SHADER_COMPILER_NAME STREQUAL "glslc")
            add_custom_command(
                OUTPUT ${SPIRV_OUTPUT}
                COMMAND ${SHADER_COMPILER}
                    --target-env=vulkan1.2
                    -fshader-stage=compute
                    -o ${SPIRV_OUTPUT}
                    ${SHADER_SRC}
                DEPENDS ${SHADER_SRC}
                COMMENT "Compiling shader ${SHADER_NAME}.comp.glsl with glslc"
            )
        else()
            add_custom_command(
                OUTPUT ${SPIRV_OUTPUT}
                COMMAND ${SHADER_COMPILER}
                    -V
                    --target-env vulkan1.2
                    -o ${SPIRV_OUTPUT}
                    ${SHADER_SRC}
                DEPENDS ${SHADER_SRC}
                COMMENT "Compiling shader ${SHADER_NAME}.comp.glsl with glslangValidator"
            )
        endif()

        add_custom_command(
            OUTPUT ${HEADER_OUTPUT}
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/spirv_to_header.py
                ${SPIRV_OUTPUT}
                ${HEADER_OUTPUT}
                ${SHADER_NAME}
            DEPENDS ${SPIRV_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/spirv_to_header.py
            COMMENT "Converting ${SHADER_NAME}.spv to C++ header"
        )

        list(APPEND SHADER_HEADERS ${HEADER_OUTPUT})
    endforeach()
endif()

# Generate dispatch table from xr.xml
set(GENERATED_DISPATCH_H "${CMAKE_CURRENT_BINARY_DIR}/xr_generated_dispatch_table.h")
set(GENERATED_DISPATCH_CPP "${CMAKE_CURRENT_BINARY_DIR}/xr_generated_dispatch_table.cpp")

add_custom_command(
    OUTPUT ${GENERATED_DISPATCH_H} ${GENERATED_DISPATCH_CPP}
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_dispatch.py
        --xr-xml ${CMAKE_CURRENT_SOURCE_DIR}/external/OpenXR-SDK/specification/registry/xr.xml
        --output-dir ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_dispatch.py
        ${CMAKE_CURRENT_SOURCE_DIR}/external/OpenXR-SDK/specification/registry/xr.xml
    COMMENT "Generating OpenXR dispatch table"
)

# Source files
set(MONOEYE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer_negotiation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer_instance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layer_proc_addr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dispatch_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hooks_frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hooks_swapchain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hooks_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hooks_view.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/swapchain_tracker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/warp_pipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/overlay_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_impl_vulkan.cpp
)

# Build the API layer shared library
add_library(monoeye SHARED
    ${MONOEYE_SOURCES}
    ${GENERATED_DISPATCH_H}
    ${GENERATED_DISPATCH_CPP}
)

target_include_directories(monoeye PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)

# Build the OpenVR Proxy DLL (Windows Only)
if(WIN32)
    add_library(openvr_api SHARED 
        src/openvr_proxy/openvr_proxy.cpp
        src/logging.cpp
    )
    target_include_directories(openvr_api PRIVATE external/openvr/headers)
    target_link_libraries(openvr_api PRIVATE Vulkan::Vulkan)
    set_target_properties(openvr_api PROPERTIES PREFIX "") # Remove 'lib' prefix
endif()

if(MONOEYE_EMBED_SHADERS)
    target_sources(monoeye PRIVATE ${SHADER_HEADERS})
    target_compile_definitions(monoeye PRIVATE MONOEYE_EMBED_SHADERS=1)
endif()

target_include_directories(monoeye PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(monoeye PRIVATE
    openxr_headers
    Vulkan::Vulkan
    Threads::Threads
)


target_compile_definitions(monoeye PRIVATE
    MONOEYE_EXPORTS
    XR_USE_GRAPHICS_API_VULKAN
    $<$<PLATFORM_ID:Windows>:XR_USE_PLATFORM_WIN32>
    $<$<PLATFORM_ID:Windows>:XR_USE_GRAPHICS_API_D3D11>
    $<$<PLATFORM_ID:Windows>:XR_USE_GRAPHICS_API_D3D12>
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)

# Windows-specific settings
if(WIN32)
    set_target_properties(monoeye PROPERTIES
        PREFIX ""
        OUTPUT_NAME "XR_APILAYER_NOVENDOR_monoeye"
    )
    # Export only the negotiation function
    target_link_options(monoeye PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/monoeye.def")
else()
    set_target_properties(monoeye PROPERTIES
        PREFIX ""
        OUTPUT_NAME "XR_APILAYER_NOVENDOR_monoeye"
    )
    target_link_options(monoeye PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/monoeye.map")
endif()

# Warnings
if(MONOEYE_STRICT_WARNINGS)
    if(MSVC)
        target_compile_options(monoeye PRIVATE /W4 /WX)
    else()
        target_compile_options(monoeye PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

# Install targets
if(WIN32)
    set(MONOEYE_INSTALL_DIR "bin")
else()
    set(MONOEYE_INSTALL_DIR "lib")
endif()

install(TARGETS monoeye
    DESTINATION ${MONOEYE_INSTALL_DIR}
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/manifest/XR_APILAYER_NOVENDOR_monoeye.json
    DESTINATION ${MONOEYE_INSTALL_DIR}
)

# Generate the JSON manifest with correct library path
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/manifest/XR_APILAYER_NOVENDOR_monoeye.json
    ${CMAKE_CURRENT_BINARY_DIR}/XR_APILAYER_NOVENDOR_monoeye.json
    @ONLY
)
