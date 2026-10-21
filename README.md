# Alan Wake 2 Path Tracing Shadows Fix

Fixes broken alpha-tested shadows on hair and foliage and missing grass shadows at LOD transitions introduced in PC version **1.2.10**. Also corrects duplicate shadows on mixed-LOD plants. Applies the fixes in memory; the game EXE stays unchanged.

[Update 1.2.10](https://www.alanwake.com/story/alan-wake-2-update-notes/) moved Opacity Micromaps (OMM) and Shader Execution Reordering to the Agility SDK for support across GPU vendors. The updated OMM path was missing a pipeline support flag and used mismatched opacity indices for clustered geometry, producing blocky shadows through transparent areas. Grass had a separate registration issue that left its static far LODs out of the ray-tracing scene. This mod corrects both paths while keeping the Agility SDK implementation.

## Install

Download the plugin from [Releases](https://github.com/Goncas2/AW2PTShadowsFix/releases).

1. Install the x64 `winmm.dll` from [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) beside the game EXE. Keep an existing compatible loader rather than overwriting it.
2. Put `AW2PTShadowsFix.asi` in the game's `plugins` folder.
3. Start the game normally. `plugins/AW2PTShadowsFix.log` reports whether the fix loaded.

Use an original, unmodified EXE. This single ASI automatically selects either supported build by its SHA-256; no setting is needed.

| EXE file version | File size | SHA-256 |
|---|---:|---|
| Previous: `0.559.302.8` | 62,304,768 bytes | `0e7a7b5fe05ada30ac043d96eeb658e934db96b4783479ade64495ed428b5904` |
| Updated: `0.564.904.9` | 61,416,960 bytes | `09135f128c07cfa4598b22dd4754a98b2c1f4ff816bfcb1e336ecfb4462b4163` |

These are EXE file versions, not the game's marketing version numbers. The log records the selected profile and whether the fix is active. The filename can differ. Other builds and conflicting patches are refused. Both supported builds have passed user testing. Testing has been limited to an RTX 4070 (original release: driver 617.42); other GPUs have not been verified.

To uninstall, close the game and remove the ASI. Keep the loader if other mods use it.

## Build

Requires Windows x64, PowerShell 7 and native MinGW-w64 GCC with UCRT.

```powershell
.\build.ps1
```

Output: `build/AW2PTShadowsFix.asi`. Add `-Package` to create a loader-free release ZIP. Use `-Compiler` to select GCC.

Tests also require NASM and the supported original EXEs. Supply one or both:

```powershell
.\build.ps1 -RunTests -PristineExe 'C:\path\to\previous.exe','C:\path\to\updated.exe'
```

[Implementation and test notes](docs/implementation.md).

OMM corrections and mixed-LOD lifecycle work by AGY. Uses [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader), maintained by ThirteenAG; download it separately.

[MIT license](LICENSE).
