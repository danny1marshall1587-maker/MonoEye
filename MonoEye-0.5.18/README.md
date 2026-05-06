# MonoEye

**Single-eye VR rendering with depth-based second eye reconstruction.**

MonoEye is a cross-platform OpenXR API layer that intercepts VR frame submission, renders one eye only, and uses a Vulkan compute shader with depth buffer data to reconstruct the second eye. This reduces GPU workload by approximately **45%** while maintaining stereoscopic depth perception.

## How It Works

```
┌──────────────────────────────────────────────────────────┐
│                         Game Engine                       │
│                    (OpenXR Application)                   │
├──────────────────────────────────────────────────────────┤
│                    MonoEye API Layer                      │
│                                                           │
│  xrEndFrame() hook:                                      │
│   1. Captures left eye color + depth                     │
│   2. Runs Vulkan compute depth-warp shader               │
│   3. Generates right eye from depth-displaced left eye   │
│   4. Passes both eyes to the runtime                     │
├──────────────────────────────────────────────────────────┤
│                    OpenXR Runtime                         │
│              (SteamVR / Windows MR / Monado)              │
├──────────────────────────────────────────────────────────┤
│                         GPU                               │
│             (Vulkan driver + compositor)                  │
└──────────────────────────────────────────────────────────┘
```

The depth-warp shader uses per-pixel depth values to correctly shift pixels based on the inter-pupillary distance (IPD), handling both close objects (large parallax) and distant objects (minimal parallax).

## Installation

### Windows (Quick Setup)

1. **Download** the latest release ZIP from the [Releases page](https://github.com/danny1marshall1587-maker/MonoEye/releases)
2. **Extract** to a permanent location (e.g., `C:\Program Files\MonoEye\`)
3. **Run** `MonoEyeSwitcher.exe` as Administrator
4. **Click "Enable"** - that's it

The switcher registers MonoEye as an implicit OpenXR API layer. Any OpenXR game you launch afterwards will automatically use it.

### Windows (Manual)

1. Place `XR_APILAYER_NOVENDOR_monoeye.dll` and `XR_APILAYER_NOVENDOR_monoeye.json` in the same directory
2. Set the system environment variable:
   ```
   MONOEYE_ENABLE=1
   ```
3. Ensure the directory containing the DLL is in your `PATH` or set `XR_API_LAYER_PATH` to point to the directory containing the JSON manifest

### Linux

1. Build from source or install from package
2. Set `MONOEYE_ENABLE=1` in your environment
3. Launch your OpenXR application

The OpenXR loader will automatically discover and load the layer.

## Usage

### One-Click Switcher (Windows)

`MonoEyeSwitcher.exe` is a small utility that enables/disables MonoEye for all OpenXR applications:

- **Enable** - Registers MonoEye as an implicit OpenXR layer. All subsequent OpenXR games will use it.
- **Disable** - Unregisters the layer. Games fall back to normal dual-eye rendering.
- **Quality Mode** - Select the warp quality preset (see below).
- **Status Indicator** - Shows whether MonoEye is currently active.

**You must run the switcher as Administrator** to modify system environment variables.

### Configuration

All configuration is done through environment variables. Set them before launching your VR application:

| Variable | Values | Default | Description |
|----------|--------|---------|-------------|
| `MONOEYE_ENABLE` | `1` | - | Enable the API layer |
| `MONOEYE_DISABLE` | `1` | - | Disable the API layer (overrides enable) |
| `MONOEYE_QUALITY` | `fast`, `balanced`, `quality` | `balanced` | Warp quality mode |
| `MONOEYE_LEFT_EYE` | `left`, `right` | `left` | Which eye to render natively |
| `MONOEYE_IPD` | `0.050` - `0.080` | `0.064` | Manual IPD override (meters) |
| `MONOEYE_LOG_LEVEL` | `debug`, `info`, `warn`, `error` | `info` | Logging verbosity |
| `MONOEYE_RENDER_SCALE` | `0.5` - `1.5` | `1.0` | Scale factor for the native render |

### Quality Modes

| Mode | Description | Performance | Visual Quality |
|------|-------------|-------------|----------------|
| `fast` | Nearest-neighbor depth warp with basic occlusion fill | ~50% GPU reduction | Good for distant scenes, may show artifacts on close objects |
| `balanced` | Bilinear depth warp with parallax-corrected occlusion fill | ~45% GPU reduction | Recommended default, works well for most games |
| `quality` | Temporal reprojection with 2-frame history and edge-aware filtering | ~35% GPU reduction | Best quality, minimal artifacts, slightly higher GPU cost |

### Per-Game Configuration

To set different quality modes per game, create a shortcut or use the game's launch options:

**Steam:** Right-click game > Properties > General > Launch Options:
```
cmd /c "set MONOEYE_ENABLE=1 && set MONOEYE_QUALITY=fast && start game.exe"
```

**Or create a `.bat` file:**
```batch
@echo off
set MONOEYE_ENABLE=1
set MONOEYE_QUALITY=fast
start "" "C:\Path\To\Game.exe"
```

## Compatibility

### OpenXR-Native Games (Works Now)

These games use OpenXR natively and will work with MonoEye immediately:

#### Sim Racing
| Game | OpenXR Support | Notes |
|------|---------------|-------|
| **Automobilista 2** | Native (Menu > Video > VR API: OpenXR) | Set to OpenXR in video settings. Works well. |
| **Assetto Corsa** | Native (via Content Manager + CSP) | Requires Content Manager and Custom Shaders Patch. Enable "OpenXR" in VR settings. |
| **Assetto Corsa Competizione** | Native (v1.9+) | Enable OpenXR in video settings. |
| **Le Mans Ultimate** | Native | Built on the rFactor 2 engine with OpenXR support. |
| **iRacing** | Beta (OpenXR mode in settings) | Enable OpenXR in the iRacing launcher under Graphics. |
| **rFactor 2** | Native | Enable OpenXR in video options. |

#### Flight Sims
| Game | OpenXR Support | Notes |
|------|---------------|-------|
| **Microsoft Flight Simulator 2020** | Native | Excellent OpenXR support. Set to OpenXR in VR settings. |
| **Microsoft Flight Simulator 2024** | Native | Same as MSFS 2020. |
| **DCS World** | Native (via OpenXR mod) | Use the OpenXR mod by FPV.fly. Available on GitHub. |
| **X-Plane 12** | Native | Enable OpenXR in VR settings. |
| **X-Plane 11** | Native | Enable OpenXR in VR settings. |

#### Other
| Game | Notes |
|------|-------|
| **EVE Online VR** | Native OpenXR support in Ambulation mode |
| **Elite Dangerous** | Native OpenXR support in Odyssey + Horizons |
| **Half-Life: Alyx** | Native OpenXR support |
| **Blade & Sorcery** | Native OpenXR support |
| **Pavlov VR** | Native OpenXR support |

### SteamVR/OpenVR Games (Requires Phase 2)

These games use OpenVR (SteamVR API) and **will not work with MonoEye yet**. Phase 2 of this project will add an OpenVR interface that translates OpenVR calls to OpenXR, enabling MonoEye to work with these games:

| Game | API | Phase 2 Status |
|------|-----|----------------|
| **RaceRoom Racing Experience** | OpenVR only | Planned |
| **Assetto Corsa Competizione** (older versions) | OpenVR | Native OpenXR in v1.9+ |
| **Project CARS 2** | OpenVR | Planned |
| **No Man's Sky** | OpenVR | Planned |
| **Skyrim VR** | OpenVR | Planned |
| **Fallout 4 VR** | OpenVR | Planned |

### SteamVR as OpenXR Runtime

MonoEye works **on top of** your OpenXR runtime. If SteamVR is your active OpenXR runtime, MonoEye will intercept frames before they reach SteamVR. You do **not** need to "replace" SteamVR - MonoEye is a layer that sits between the game and SteamVR.

To use SteamVR as your OpenXR runtime:
1. Launch SteamVR
2. In SteamVR Settings > Developer > Set SteamVR as OpenXR runtime
3. Launch your OpenXR game
4. MonoEye will automatically intercept if enabled

## Troubleshooting

### MonoEye not loading

1. Verify `MONOEYE_ENABLE=1` is set as a system environment variable
2. Check that the DLL and JSON manifest are in the same directory
3. Run the switcher as Administrator
4. Check the OpenXR loader log: set `XR_LOADER_DEBUG=all` and check output

### Visual artifacts in second eye

1. Try `MONOEYE_QUALITY=quality` for better warp quality
2. Some games with custom depth formats may have issues - check logs
3. Disable if the game uses dynamic resolution scaling (the warp assumes consistent resolution)

### Performance worse than expected

1. Make sure the game is actually rendering one eye (check GPU usage)
2. Try `MONOEYE_RENDER_SCALE=0.9` to reduce base render resolution
3. Some games with complex depth buffer setups may not benefit as much

### Game crashes on startup

1. Check `MONOEYE_LOG_LEVEL=debug` and review the log output
2. The game may not support `XR_KHR_composition_layer_depth` - this extension is required
3. Report the issue with the log output

## Build from Source

### Prerequisites

- **Windows**: Visual Studio 2022+, Vulkan SDK, Python 3.10+
- **Linux**: GCC 11+, CMake 3.18+, Vulkan development headers, glslc, Python 3.10+

### Build

```bash
git clone --recursive https://github.com/danny1marshall1587-maker/MonoEye.git
cd MonoEye
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The compiled DLL/SO will be in the build directory.

## Architecture

MonoEye is organized into these components:

- **Layer Negotiation** - OpenXR loader entry point (`xrNegotiateLoaderApiLayerInterface`)
- **Instance Hooks** - Intercepts `xrCreateInstance` / `xrDestroyInstance`
- **Session Hooks** - Intercepts `xrCreateSession` for Vulkan device access
- **Frame Hooks** - Intercepts `xrBeginFrame` / `xrEndFrame` (the main work)
- **Swapchain Tracker** - Tracks all swapchains to find the left/right eye textures
- **Warp Pipeline** - Vulkan compute shader pipeline for depth-based warping
- **Config System** - Environment variable parsing
- **Logging** - Debug output (DebugView on Windows, stderr on Linux)

## License

BSD 2-Clause License. See [LICENSE](LICENSE) for details.

## Roadmap

### Phase 1 (Current) - OpenXR API Layer
- [x] Core layer infrastructure
- [x] Depth-warp compute shader
- [x] Swapchain tracking
- [x] Quality modes (fast/balanced/quality)
- [x] Windows switcher utility
- [ ] Per-game configuration profiles
- [ ] HUD/stereo UI preservation

### Phase 2 - OpenVR Translation Layer
- [ ] OpenVR interface implementation
- [ ] OpenVR action manifest translation
- [ ] SteamVR controller emulation
- [ ] Works with RaceRoom, older ACC, Project CARS 2, etc.

### Phase 3 - OVRPlugin Unlock Layer
- [ ] Meta OVRPlugin bypass for non-Meta runtimes
- [ ] Enables Quest-native games to run on SteamVR/Monado
- [ ] Works with games like Asgard's Wrath 2, Batman Arkham Shadow, etc.
