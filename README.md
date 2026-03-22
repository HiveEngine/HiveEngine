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

## Gameplay Plugin

Game logic lives in a separate DLL loaded at runtime by the engine launcher.
The project scaffolder generates a CMake project with hot-reload support —
modify your gameplay code, recompile, and the engine picks up changes
without restarting.

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

Presets: `hive-game`, `hive-editor`, `hive-headless`.

## Status

Early development. The ECS, allocators, containers, asset pipeline, and editor
scaffolding are functional. Rendering is minimal (triangle-level).

## License

Licensed under the [Apache License 2.0](LICENSE).

### Third-Party Libraries

See [NOTICE](NOTICE) for the full list. The editor links Qt 6 dynamically
under LGPL v3. The game runtime does not use Qt.
