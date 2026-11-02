# QEMU / host-common integration gap (2026-10-30)

## Short answer

Yes, files can be added to this repo with the current token (repo +
workflow scopes, owner asrussetrose-altu). Two files were already
pushed today from this takeover: the probe fix and the GLES 3.2
capability gate. No fork is needed: this is your own repo, so pushes
go directly to branch `gles32-negotiation`.

## What GPT meant by "add some from qemu"

The missing exports in the standalone DLL are emulator lifecycle
entry points, not translator code:

- `android_setOpenglesRenderer`
- `android_stopOpenglesRenderer`

plus the GPU-selection hints `AmdPowerXpressRequestHighPerformance`
and `NvOptimusEnablement` (349 original exports vs 345 standalone).

In the AOSP layout, the renderer lifecycle code lives in
`gl-host-common/opengles.cpp` (device/generic/vulkan-cereal), which
provides `android_setOpenglesEmulation`, `android_startOpenglesRenderer`,
and the render-lib handoff. In the parent emulator tree, the same
integration is wired through `external/qemu/android/android-emu/android/opengles.cpp`.

## Why copying one file alone will not work

Checked 2026-10-30: this repo has NO `gl-host-common` folder at all
(not in `host/`, not in `host/gl/`). `gl-host-common/opengles.cpp`
is not self-contained. It includes, among others:

- `host-common/opengles.h`, `host-common/address_space_device.h`,
  `host-common/FeatureControl.h`, `host-common/globals.h`
- `aemu/base/*` (GLObjectCounter, Stream, PathUtils, SharedLibrary,
  System, MemoryTracker)
- `host-common/opengl/*` (emugl_config, GLProcessPipe, logger, gpuinfo)
- `OpenGLESDispatch/*`, `render-utils/render_api_functions.h`

Copying only `opengles.cpp` into this repo would fail to compile
until all of those headers and libraries are also present, and the
standalone CMake build deliberately clears `GFXSTREAM_HOST_COMMON_LIB`
because the parent build is supposed to supply it.

## Correct route

Do NOT add a lone `opengles.cpp` to this repo and expect a working
DLL. The correct route, already recorded in `docs/GLES32_PORT_STATUS.md`,
is:

1. Build through the parent emulator `external/qemu` tree, where
   `gl-host-common` and the aemu libraries already exist and are
   linked into the backend.
2. That build is what supplies the two `android_*` exports and the
   emulator-specific imports (`libandroid-emu-*.dll`, `libabseil_dll.dll`,
   `liblibprotobuf.dll`) seen in the original 37.2.12 DLL.
3. Compare the resulting PE against `orig_dll/libgfxstream_backend.dll`
   (22,858,904 bytes, 349 exports) before any replacement.
4. The standalone CMake target remains compile-only validation.
   It is not an installable replacement for Emulator 37.2.12.

## What is safe to add here

- Documentation like this file (no build impact).
- Translator / detector / render-control changes that compile in the
  standalone check (already done for the gate and probe fix).
- Small, self-contained tools like `tools/windows-gl-capability-probe.cpp`.

Any host-common file added here must come with its full dependency
tree and a CMake/Bazel wiring change, and must be compile-checked
in Actions before it is treated as progress.
