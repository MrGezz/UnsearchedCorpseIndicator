# UnsearchedCorpsesIndicator — SSE 1.7.104 Port Notes

Updated: 2026-09-12

## Status

**Patched, not yet built.** One source fix applied (see below). Not deployed; blocked on
StatusIndicatorFramework (SIF) being re-enabled first — this plugin registers conditions
with SIF and depends on it loading cleanly.

## CommonLibSSE lineage

- Package: `commonlibsse-ng` (CharmedBaryon's lineage)
- Registry: `https://gitlab.com/colorglass/vcpkg-colorglass`
- Baseline pinned in `vcpkg-configuration.json`: `6fb127f7d425ae3cf3fab0f79005d907c885c0d8`
- Format-5 Address Library support: CommonLibSSE-NG has had `Format::SSEv5` /
  `_id2offsetDense` since upstream commit `7bc2aaa` (colorglass package commit `e7863a7`).
  **Verify at configure time** that the resolved `REL/ID.h` in
  `D:\b\uci\vcpkg_installed\...\include\REL\ID.h` contains `Format::SSEv5`. If it does not,
  bump the colorglass baseline to a known-good one (e.g. `eb2f2bd143b8f7c52b57ef5a822163b56f7be7d6`
  from the KID build, or the most recent on the colorglass repository).

## AE compatibility

`plugin.cpp` uses the `SKSEPluginLoad(...)` macro from CommonLibSSE-NG. This lineage always
exports `SKSEPlugin_Version` + `SKSEPlugin_Load` (the AE API) regardless of CMake flags.
`BUILD_SKYRIMAE=TRUE` is NOT needed here — that flag is specific to po3's CommonLibSSE.
`add_commonlibsse_plugin()` auto-generates the `SKSEPlugin_Version` struct from the project
version declared in `CMakeLists.txt` (`VERSION 1.3.2`).

## CMakePresets

No `vs2026-ae` or `vs2026-se` separation. Uses a single Ninja-based `release` preset with
triplet `x64-windows-static-md`. This is correct for CommonLibSSE-NG.

The preset's `binaryDir` is `${sourceDir}/build/${presetName}` (inside the checkout). Override
with `-B D:/b/uci` on the command line to stay on D: and avoid MAX_PATH in vcpkg buildtrees.

There is no `cmake/` overlay-triplets directory in the repo; the `VCPKG_OVERLAY_TRIPLETS`
entry in the preset references `${sourceDir}/cmake` which is absent and harmless (vcpkg
ignores a missing overlay path).

## Source patches applied

### 1. `src/Events.cpp` — null guard on `ScriptEventSourceHolder::GetSingleton()`  (2026-09-12)

`RE::ScriptEventSourceHolder::GetSingleton()` was dereferenced without a null check inside
the `kDataLoaded` message handler. Although in practice the singleton is always valid by
`kDataLoaded`, the same pattern (unguarded singleton dereference in an event callback) was
the bug class that caused crashes in StatusIndicatorFramework. Added:

```cpp
if (!holder) {
    SKSE::log::error("ScriptEventSourceHolder singleton is null at kDataLoaded");
    return;
}
```

No other callbacks access a game singleton without a guard:
- `ContainerChangedSink::ProcessEvent` null-checks `RE::PlayerCharacter::GetSingleton()`.
- `InputSink`, `DeathSink`, `ActivateSink`: no singleton access.
- `HasLootCondition::Match`, `NotSearchedCondition::Match`: no singleton access.

## Build command (PowerShell — do NOT use Bash/Git Bash)

```powershell
$SRC = "C:\Users\IceCreamAssasin\Claude\Projects\IcZ Skyrim\UnsearchedCorpseIndicator"
cmake -S $SRC -B D:\b\uci --preset release `
  -DCMAKE_TOOLCHAIN_FILE="C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DCOPY_BUILD=OFF
cmake --build D:\b\uci --config Release
```

After building, verify exports:
```powershell
dumpbin /exports D:\b\uci\Release\UnsearchedCorpsesIndicator.dll | Select-String "SKSEPlugin"
```
Expected: `SKSEPlugin_Version` and `SKSEPlugin_Load` (NOT `SKSEPlugin_Query`).

## Runtime dependencies

- `StatusIndicatorFramework.dll` must load first (SIF sends `kMessage_GetAPI` to registered
  listeners). This plugin is a no-op if SIF is absent or crashes — but SKSE log will show
  the listen registration. Enable both in MO2 together.
- No Address Library ID translation needed in this plugin's source — it contains no direct
  address offsets.
- Data files: `SKSE/Plugins/UnsearchedCorpsesIndicator.ini`,
  `SKSE/Plugins/SIF/UnsearchedCorpsesIndicator.json`,
  `Interface/UnsearchedCorpsesIndicator/indicator.swf`.

## Known issues / open items

- SIF dependency: blocked until StatusIndicatorFramework is patched and re-enabled.
- Format-5 verification: unconfirmed locally (no network access to colorglass registry);
  must check `REL/ID.h` after first cmake configure resolves the package.
