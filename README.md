# SnapOpen

Speeds up door and container opening (animation + the loading-screen fades around it in case of doors), making each pretty much instantaneous.

Built on [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) (submodule, pinned to v8.1.0). GPL-3.0, see LICENSE

## Requirements

- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444), matching your game version
- [SKSE64](https://skse.silverlock.org/)

## Config

`SnapOpen.ini`, next to the dll:

- `bOverrideFadeSettings` - also shortens a bunch of Skyrim.ini fade/loading durations. 0 to disable overriding the ini.
- `bExperimentalNPCDoors` - speeds doors up for NPCs too. Off by default, needs more testing.
- `bEnableLogging` - 0 to stop writing SnapOpen.log entirely.

## Building

VS2022 (C++ workload) + CMake + Ninja + vcpkg (`VCPKG_ROOT` set).

Clone with `--recursive`, CommonLibSSE-NG is a submodule.

```
cmake --preset debug
cmake --build --preset debug
```

Set `SKYRIM_MODS_FOLDER` (MO2/Vortex mods folder) or `SKYRIM_FOLDER` (Skyrim install) as an env var to auto-deploy the dll on build.

## Debugging

`.vscode/launch.json` has an attach config, launch via SKSE first, then attach to `SkyrimSE.exe`.
