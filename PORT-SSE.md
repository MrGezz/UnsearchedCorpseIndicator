# UnsearchedCorpsesIndicator — SSE 1.7.104 Port Notes

Updated: 2026-09-13. Branch `IcZ_SSE`, upstream `7a5bdf2`.

## Status

**Rebuilt 2026-09-12/13, deployed and enabled in MO2** under mod "Unsearched Corpse
Indicator". Runtime: Skyrim SE 1.7.104 / SKSE 2.3.1. PDB staged alongside DLL; CrashLoggerSSE
can print source file and line from it.

Requires StatusIndicatorFramework (SIF): UCI registers conditions with SIF at `kDataLoaded`.
SIF must be loaded and enabled in MO2 for UCI to do anything. Both are currently enabled.

In-game validation confirmed 2026-09-13 (see section below).

## CommonLibSSE lineage and the format-5 problem

Upstream vcpkg-configuration.json pins the colorglass registry at baseline
`6fb127f7d425ae3cf3fab0f79005d907c885c0d8`. The registry's HEAD is `6309841a`, dated
2023-05-13, serving `commonlibsse-ng` 3.7.0 / 3.8.0 (CommonLibSSE-NG commit `b93280e`). That
version has two defects on Skyrim 1.7.104:

1. `REL::Module::load_version()` classifies the runtime by `switch (_version[1])` (the minor
   version): `case 6` → AE, `default` → SE. Skyrim 1.7.x has minor **7**, so the library
   believes it is on SE, requests the Address Library in Format 1
   (`Data/SKSE/Plugins/version-<ver>.bin`), and selects the SE id in every `RELOCATION_ID(se,
   ae)` pair. The result is every native call dispatched to the wrong offset.

2. CommonLibSSE-NG `b93280e` has `enum class Format { SSEv1, SSEv2, VR }` — no `SSEv5`. The
   1.7.104 Address Library ships only as dense format 5 (`versionlib-1-7-104-0.bin`, 565,759
   slots). Even with defect 1 corrected, the library cannot parse it without format-5 support.

The colorglass registry has no newer baseline to bump to; upgrading off it is a larger task.
**The fix is a local overlay port** rather than an upgrade.

## Overlay port: `overlayports/commonlibsse-ng/`

The local overlay port is at `overlayports/commonlibsse-ng/` in this repo root.
It is wired in via `vcpkg-configuration.json`'s `"overlay-ports": ["./overlayports"]` entry.

`overlayports/commonlibsse-ng/vcpkg.json`:
- Name `commonlibsse-ng`, `version-semver: "3.8.0"`, `port-version: 2` — the bumped
  port-version changes the vcpkg ABI hash so the cached colorglass build is not reused.

`overlayports/commonlibsse-ng/portfile.cmake`:
- Fetches CharmedBaryon/CommonLibSSE at **`b93280e832f263dbef44e44cbe2936622a02f91a`**
  (the same commit colorglass served, so no source divergence).
- Applies **`commonlibsse-ng-b93280e-1.7.x-address-library-format5.patch`** before
  configuring.

The patch (canonical copy:
`Project Improvement/Status-Indicator-Framework-SKSE/.buildenv/overlayports/commonlibsse-ng/commonlibsse-ng-b93280e-1.7.x-address-library-format5.patch`)
makes three changes to `b93280e`:

- `include/REL/Module.h`: adds `case 7:` alongside `case 6:` in both `load_version()` and
  `mock()` so Skyrim 1.7.x is classified as AE, not SE.
- `include/REL/ID.h`: `header_t` accepts format 5 when 2 was requested and parses its
  96-byte tail (64-byte fixed name, `pointerSize`, `reserved dataFormat`, `count`); adds
  `_format`/`format()` accessors; `unpack_file()` gains a dense branch that skips zero-offset
  (unassigned) slots and writes the real entry count into a sentinel slot at `base[denseCount]`;
  `id2offset()` verifies `it->id == a_id` on every runtime (upstream only did so under VR).
- `src/REL/ID.cpp`: `load_file()` sizes the shared mapping to `address_count + 1` for format
  5, names it `CommonLibSSEOffsets-v5f-` (not `-v2-`) to distinguish the patched layout,
  reads the entry count back from the sentinel slot on the mmap-reuse path, and (port-version
  2) re-unpacks the file when that sentinel still reads 0 — closing the window where a plugin
  that joined the shared mapping mid-unpack would receive a zero-length span and fail every
  address lookup.

## Build

CMakePresets.json defines a Ninja-based `release` preset but the build did **not** use
presets. Actual generator: `Visual Studio 18 2026` / v145 toolset (confirmed from
`D:\b\uci\CMakeCache.txt`: `CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026`).

Commands, run from PowerShell (never Git Bash — MSYS rewrites `/O2` into a path):

    cmake -S "<repo root>" -B D:\b\uci -G "Visual Studio 18 2026" -T v145 -A x64 `
        -DCMAKE_TOOLCHAIN_FILE="C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake" `
        -DVCPKG_TARGET_TRIPLET=x64-windows-static-md `
        -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<Config:Debug>:Debug>DLL" `
        -DCMAKE_CXX_FLAGS="/permissive- /Zc:preprocessor /EHsc /MP /W4 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE"

    cmake --build D:\b\uci --config RelWithDebInfo

Binary dir `D:\b\uci` is **mandatory**: vcpkg buildtrees hit MAX_PATH when the binary dir
is inside the checkout.

Config: RelWithDebInfo (`/O2 /Ob1 /DNDEBUG`, confirmed from `CMAKE_CXX_FLAGS_RELWITHDEBINFO`
in the cache; `/Zi` added by VS for RelWithDebInfo). The `CMakeLists.txt` PDB staging
command fires only for the `Debug` config in the multi-config VS generator, so the PDB was
manually placed alongside the DLL in the staged tree.

This lineage (CharmedBaryon/CommonLibSSE, runtime dispatch via `RELOCATION_ID`) does not
use `BUILD_SKYRIMAE=TRUE`; that switch is specific to po3's CommonLibSSE fork.
`BUILD_SKYRIMAE` has no entry in `D:\b\uci\CMakeCache.txt`.

Output: `D:\b\uci\RelWithDebInfo\UnsearchedCorpsesIndicator.dll`, version 1.3.2 per
`CMakeLists.txt`, **2,376,192 bytes** (verified). The UTF-16LE string
`"CommonLibSSEOffsets-v5f-"` was found in the deployed DLL, confirming the overlay patch
was active at build time.

## Staging and deployment

`CMakeLists.txt` defaults `OUTPUT_FOLDER` to
`${CMAKE_CURRENT_SOURCE_DIR}/Skyrim/Data`. The post-build commands copy:

- `SKSE/Plugins/UnsearchedCorpsesIndicator.dll`
- `SKSE/Plugins/UnsearchedCorpsesIndicator.ini`
- `SKSE/Plugins/SIF/UnsearchedCorpsesIndicator.json`
- `Interface/UnsearchedCorpsesIndicator/indicator.swf`

`RequiemLotDPatch/tools/deploy.py` (dry run by default) syncs `Skyrim/Data` into the MO2
mod folder. Pairing in `RequiemLotDPatch/tools/deploy-map.json`:
- key: `"Unsearched Corpse Indicator"`
- `"src"`: `UnsearchedCorpseIndicator/Skyrim/Data`
- Note in the map: "Rebuilt 2026-09-12 against a PATCHED commonlibsse-ng b93280e overlay
  port … SIF.json has no repo source; the copy staged here came from the MO2 mod and must
  be kept in sync by hand."

MO2 is the deployment authority. Nothing writes into the game directory directly.

`data/UnsearchedCorpsesIndicator.json` is tracked in this repo (`git ls-files` confirms).
CMakeLists.txt copies it from that tracked source path. The same applies to
`data/UnsearchedCorpsesIndicator.ini` and `icon/indicator.swf`: all three staging inputs
are tracked. The `deploy-map.json` note "SIF.json has no repo source" predates the tracking
and is outdated; the source of record is `data/UnsearchedCorpsesIndicator.json`.

## Source fix committed: `src/Events.cpp` null guard (7a5bdf2, 2026-09-12)

`RE::ScriptEventSourceHolder::GetSingleton()` was dereferenced without a null check inside
the `kDataLoaded` message handler. Same defect class that caused crashes in SIF. Added a null
guard that logs and returns early. No other callbacks in this source access a singleton without
a guard (`ContainerChangedSink::ProcessEvent` already null-checks `PlayerCharacter::GetSingleton()`).

## What this port changed

On top of `7a5bdf2`, porting onto the patched CommonLib changed three files and added the
overlay port:

**Changed**
- **`.gitignore`**: adds `Skyrim/`, `build/` and `install/`. Everything under `Skyrim/` is build
  output (see below), so none of it is versioned.
- **`CMakeLists.txt`**: replaces the placeholder `# set(OUTPUT_FOLDER "C:/path/to/any/folder")`
  with an `if(NOT DEFINED OUTPUT_FOLDER)` block defaulting to `${CMAKE_CURRENT_SOURCE_DIR}/Skyrim/Data`.
  Carries a comment explaining the MO2 deployment authority rule.
- **`vcpkg-configuration.json`**: adds `"overlay-ports": ["./overlayports"]` at the top level
  to wire in the local overlay port. Without this entry vcpkg ignores `overlayports/`.

**Added**
- **`overlayports/commonlibsse-ng/`**: three files — `portfile.cmake`, `vcpkg.json`
  (port-version 2), and `commonlibsse-ng-b93280e-1.7.x-address-library-format5.patch`
  (189 lines). Described in the Overlay port section above.
`Skyrim/Data/` is the staged distributable tree and is deliberately not versioned: the build's
POST_BUILD steps copy the DLL into it (the PDB beside it was placed by hand, see Build), together with byte-identical copies of the
tracked `data/UnsearchedCorpsesIndicator.json`, `data/UnsearchedCorpsesIndicator.ini` and
`icon/indicator.swf`. Versioning those copies as well would keep every asset twice. `deploy.py`
syncs the tree into the MO2 mod.

## Requiem / LotD impact

None. No ESP; UCI adds no records. The DLL attaches to the SIF HUD system and tracks
container-access events. No gameplay records, combat or AI touched.

## In-game validation (2026-09-13)

Session 02:20–02:40 (skse64.log last-write 02:40:07 AM):
- "checking plugin UnsearchedCorpsesIndicator.dll" and
  `loading plugin "UnsearchedCorpsesIndicator"` both appear; the final summary line records
  `plugin UnsearchedCorpsesIndicator.dll ... loaded correctly (handle 22)`. No error,
  unsupported, or fail lines anywhere in that log.

StatusIndicatorFramework.log 02:20:47:
- `SIF API: registered 'hasLoot'`
- `SIF API: registered 'notSearched'`
- `[UnsearchedCorpsesIndicator.json] Loaded 1 rules`
- `Loaded 1 config files, 1 rules total`

This is the SIF+UCI handshake observed in a live session: UCI's conditions registered with
SIF and SIF loaded UCI's rule file at `kDataLoaded`.

No crash log exists after `crash-2026-09-13-00-03-14.log` (verified). That run's SKSE
PLUGINS section has no `UnsearchedCorpsesIndicator.dll` entry — it was a grass-precache
run that predates this plugin being enabled.

AA (the user) reports playing up to the Legacy of the Dragonborn Dragonborn Gallery intro
with no issues.

**Not yet observed in any log**: the skull indicator drawn over an unsearched corpse.

## Adversarial review

Three independent reviewers examined the patch:

1. **Dense-reader correctness.** The `lower_bound` miss-returns-neighbour defect, the
   zero-offset hole filter, the sentinel slot, and the `it->id == a_id` guard on every
   runtime were all verified correct. 40 AE ids spot-checked against
   `versionlib-1-7-104-0.bin`; all present.

2. **Shared-mapping lifecycle.** The open/create race (a second plugin could open the
   mapping before the first finishes unpacking) is pre-existing and harmless in practice:
   SKSE's `PluginManager::InstallPlugins` loads plugins in a single-threaded loop. The
   port-version 2 re-unpack guards the zero-sentinel edge case for the rare double-unpack
   path.

3. **Blast radius of the AE reclassification.** The two ids absent from 1.7.104 (`82331`,
   `99886`) are unreachable from this plugin's sources (`grep RELOCATION_ID src/ include/`:
   zero hits). The 1.7.x-is-AE reclassification affects only the Address Library lookup path.

All three returned **SOUND** with no required changes.
