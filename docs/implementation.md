# Implementation

## OMM

Three in-place edits restore pipeline OMM support, clear EAX before assembling capability flags, and use the cluster-ordered OMM index stream. The EAX edit supports the compact pipeline-options rewrite. All three are retained; the tester found that removing the index correction regressed the result.

The edits keep the game's Agility OMM implementation. No shader replacements or feature-disabling changes are included.

## Grass and mixed LODs

Some plants have deforming near LODs and static far LODs. The original registration path chooses one registration class for the whole object. When it chooses dynamic registration, the static tail never gets registered. The deforming BLAS is removed at the LOD transition, leaving no ray-traced representation to cast shadows.

The wrapper runs the original registrar, collects existing static model handles for non-deforming LODs, and registers them through the original static manager. Eligibility follows the engine's deformation-entry clamping rule. Material variants and per-object overrides are preserved. There are no filename, distance or object-size filters; the supported bounds are eight LODs and sixteen model handles.

Both cleanup paths remove both registrations. Both object-dirty and visibility notification paths also mark static records dirty, so the original TLAS logic can evict a far static LOD when a near deforming LOD becomes active. Omitting those notifications caused the duplicate fern-shadow artifact in the registration-only fix.

## Runtime patching

`src/aw2_builds.c` checks the disk EXE's size and SHA-256 and selects one of two immutable profiles. `src/plugin.c` logs the selected EXE file version, then passes that profile to the patcher. `src/aw2_core.c` validates the loaded PE, all eight original instruction spans, the original scheduling controls and the parent unwind fragment using that profile. The executable's filename is unrestricted. There is no signature-scanning fallback for unknown builds.

The helper payload is allocated near the game image. Five references into the game are relocated; internal branches remain unchanged. Two runtime-function entries are registered with `RtlAddFunctionTable`, using the game image base. The notification helper chains to the parent body fragment that describes the saved R12/R15 registers. It does not chain to the earlier prologue-only fragment.

Validation, allocation and unwind registration complete before the hooks are written. Page protections are restored in reverse order because some patches share a page. The plugin and helper allocation stay resident until process exit. Initialization must occur before rendering pipelines and scene objects are created; no hot attach or hot unload is supported.

The two later AGY scheduling edits are excluded. Shaders, assets and LOD distances are unchanged. No performance improvement has been measured.

## Payload source

`src/grass.asm` is the shared NASM source for the 496-byte payload. `data/supported-builds.json` records each build's identity, eight instruction spans, five relocations, scheduling guards and parent unwind record. The generator supplies each profile's addresses to the assembler. `src/aw2_data.h` contains the two generated profiles and is checked in, so building the ASI requires neither NASM nor a game executable.

To regenerate it, install Python 3 and NASM, then run:

```powershell
python tools/generate_data.py
```

Offsets and the chained unwind descriptor belong to each exact executable layout. Adding another game build requires reviewing its functions and metadata, rather than adding its file hash alone. The two profiles reproduce the previously verified single-build payloads byte-for-byte at their reference addresses. Both include OMM 1+2+3, the complete grass/mixed-LOD lifecycle fix and corrected unwind metadata; neither includes the later scheduling edits.

## Verification

`build.ps1 -RunTests -PristineExe ...` uses a local, hash-verified original EXE to construct private test images. Original engine calls are replaced by test stubs before execution. Tests run the real relocated payload and Windows unwinder, not the game renderer.

For each EXE, the suite covers 396 registration cases, 544 notification cases, 164 unwind cases, ten transaction/range checks and eight identity checks across two image placements. It checks material variants, overrides, deformation clamping, registration classes, both notification paths, RT gating, static-record bounds and bitmap word transitions. It also checks real-file hash selection, unknown hashes/sizes, unreadable files and wrong-profile refusal without any image writes.

Both supported builds have passed the native suite and user testing. Testing has been limited to an RTX 4070; the original release was tested with driver 617.42. Other GPU vendors and drivers have not been verified.

The current logger recreates its log each time the plugin initializes. Another executable loading the same ASI can overwrite a previous session log; an isolated refusal line does not identify the preceding game's result.
