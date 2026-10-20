# Alan Wake 2 Path Tracing Shadows Fix

Fixes broken alpha-tested shadows on hair and foliage and missing grass shadows at LOD transitions introduced in PC version **1.2.10**. Also corrects duplicate shadows on mixed-LOD plants. Applies the fixes in memory; the game EXE stays unchanged.

[Update 1.2.10](https://www.alanwake.com/story/alan-wake-2-update-notes/) moved Opacity Micromaps (OMM) and Shader Execution Reordering to the Agility SDK for support across GPU vendors. The updated OMM path was missing a pipeline support flag and used mismatched opacity indices for clustered geometry, producing blocky shadows through transparent areas. Grass had a separate registration issue that left its static far LODs out of the ray-tracing scene. This mod corrects both paths while keeping the Agility SDK implementation.

## Install

Download the plugin from [Releases](https://github.com/Goncas2/AW2PTShadowsFix/releases).

1. Install the x64 `winmm.dll` from [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) beside the game EXE. Keep an existing compatible loader rather than overwriting it.
2. Put `AW2PTShadowsFix.asi` in the game's `plugins` folder.
3. Start the game normally. `plugins/AW2PTShadowsFix.log` reports whether the fix loaded.

Use the original EXE. The supported file is 62,304,768 bytes with SHA-256:

```text
0e7a7b5fe05ada30ac043d96eeb658e934db96b4783479ade64495ed428b5904
```

The filename can differ. Other builds and conflicting patches are refused. Tested on an RTX 4070; other GPUs have not been verified.

To uninstall, close the game and remove the ASI. Keep the loader if other mods use it.

## Build

Requires Windows x64, PowerShell 7 and native MinGW-w64 GCC with UCRT.

```powershell
.\build.ps1
```

Output: `build/AW2PTShadowsFix.asi`. Add `-Package` to create a loader-free release ZIP. Use `-Compiler` to select GCC.

Tests also require NASM and the supported original EXE:

```powershell
.\build.ps1 -RunTests -PristineExe 'C:\path\to\AlanWake2.exe'
```

[Implementation and test notes](docs/implementation.md).

OMM corrections and mixed-LOD lifecycle work by AGY. Uses [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader), maintained by ThirteenAG; download it separately.

[MIT license](LICENSE).
