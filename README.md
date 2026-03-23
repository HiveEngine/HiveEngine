# HiveEngine

C++20 game engine built from scratch. Explicit memory control,
no RTTI, no exceptions. Vulkan-only rendering via Diligent. Windows and Linux.

## Modules

| Module | Role |
|--------|------|
| **Queen** | ECS — archetype SoA, parallel scheduler, reflection, serialization |
| **Comb** | Allocators — linear, buddy, pool, stack, slab, thread-safe, debug tracking |
| **Wax** | Containers — Vector, HashMap, String, smart pointers, binary serialization |
| **Hive** | Core — logging, math (Float2/3/4, Mat4, Quat, AABB, BVH), Tracy profiling |
| **Terra** | Windowing — GLFW, Win32/X11/Wayland native handles |
| **Antennae** | Input — keyboard/mouse → ECS resources, action mapping |
| **Nectar** | Assets — VFS, glTF/OBJ import, hot-reload, cook pipeline, pak archives |
| **Swarm** | Rendering — Diligent/Vulkan backend (early) |
| **Waggle** | Engine layer — game loop, module registry, scene loading, gameplay DLL |
| **Forge** | Editor — Qt 6, hierarchy/inspector/asset browser/console/viewport |
| **Larvae** | Testing — compile-time registry, runner, benchmarks, 121 test files |

## Gameplay Plugin & Hot-Reload

Game logic lives in a separate DLL loaded at runtime by the engine launcher.
The project scaffolder generates a CMake project with hot-reload support.

### How It Works

The engine can be built as a **shared library** (`hive_engine.dll`) instead of
static archives. The gameplay DLL links against this shared library, ensuring
a single copy of all engine singletons, allocators, and global state. When the
gameplay DLL is rebuilt, the editor unloads it via `FreeLibrary`, copies the
new version, and reloads it — all without restarting.

### Quick Start

```bash
# Configure with the shared editor preset (CLion plugin or manual)
cmake --preset hive-editor-shared

# Build everything
cmake --build out/build/hive-editor-shared

# Launch the editor
out/build/hive-editor-shared/bin/Debug/hive_launcher.exe --project path/to/project.hive
```

In the editor, press **Ctrl+B** (or click the Build button in the toolbar) to
recompile the gameplay DLL. The reload happens automatically on success.

### Shared vs Static Build

| | Static (`hive-editor`) | Shared (`hive-editor-shared`) |
|---|---|---|
| Hot-reload | No (restart required) | Yes (`FreeLibrary` + reload) |
| Tracy profiler | Yes | No (incompatible with DLL unload) |
| Comb mem-debug | Yes | No (consumers use release allocators) |
| Use case | Profiling, memory debugging | Gameplay iteration |

### Gameplay DLL Constraints

For `FreeLibrary` to work cleanly, gameplay DLLs must:

- **Not use `static` allocators.** Use `new`/`delete` in `Register`/`Unregister`
  instead of function-local statics (the CRT runs atexit handlers during unload).
- **Not include `<fstream>`** or other STL headers that create module-level statics
  with non-trivial destructors. Use C-style `fopen`/`fwrite` or engine VFS APIs.
- **Not use Tracy macros directly.** The profiler is disabled for DLL consumers
  (`HIVE_FEATURE_PROFILER=0`) to avoid TLS/thread issues during unload.

### Architecture

```
hive_launcher.exe ──links──▶ hive_engine.dll  (all engine modules)
                   ──links──▶ Forge.lib        (editor UI, static)
                   ──loads──▶ gameplay.dll      (links hive_engine.dll)
                                 │
                           shadow copy: gameplay_live.dll
                           (original stays free for the linker)
```

The launcher uses a **shadow copy**: it copies `gameplay.dll` to
`gameplay_live.dll` and loads the copy. The original file stays unlocked so
the linker can overwrite it during rebuilds.

## CLion Plugin

A CLion plugin (`tools/clion-hive-plugin/`) provides a tool window for
configuring the engine build. It reads `HiveFeatures.json`, detects the
active toolchain (LLVM, MSVC, GCC), and generates `CMakeUserPresets.json`
with the selected mode (Game/Editor/Headless), build config, and feature
toggles. Works for both engine and game project workspaces.

## Building

Requires CMake 3.25+, Clang-CL (Windows) or Clang (Linux).

```bash
cmake --preset hive-editor
cmake --build out/build/hive-editor
```

Presets: `hive-game`, `hive-editor`, `hive-headless`, `hive-editor-shared`.

## Status

Early development. The ECS, allocators, containers, asset pipeline, and editor
scaffolding are functional. Rendering is minimal (triangle-level).

## License

Licensed under the [Apache License 2.0](LICENSE).

### Third-Party Libraries

See [NOTICE](NOTICE) for the full list. The editor links Qt 6 dynamically
under LGPL v3. The game runtime does not use Qt.
