# GLES 3.2 on the Windows Android Emulator backend

## Verified state (2026-10-30)

This repository is not, by itself, a drop-in source tree for the Windows
Android Emulator's `libgfxstream_backend.dll`.

The standalone CMake target in `host/CMakeLists.txt` builds the shared
`gfxstream_backend` from `render_api.cpp` and
`virtio_gpu_gfxstream_renderer.cpp`. Its standalone configuration clears
`GFXSTREAM_HOST_COMMON_LIB`. The emulator's parent build supplies additional
AEMU integration. A standalone build was already tested against Emulator
37.2.12 and failed at startup because the resulting DLL did not export the
required `android_setOpenglesRenderer` symbol (and also lacked
`android_stopOpenglesRenderer`). Do not replace the installed DLL with that
standalone artifact.

## GLES version negotiation

The guest-side feature definitions already include
`ANDROID_EMU_gles_max_version_3_2` and the guest parser recognizes it.

The host does not yet negotiate GLES 3.2:

- `host/gl/gles_version_detector.cpp` only creates GLES 3.0 and 3.1 contexts.
  Its fallback maximum is hard-coded to
  `GLES_DISPATCH_MAX_VERSION_3_1`.
- `host/render_control.cpp` advertises only the 2.0, 3.0 and 3.1 feature
  strings; `maxVersionToFeatureString()` has no 3.2 case.
- The shared `GLESDispatchMaxVersion` enum already has a
  `GLES_DISPATCH_MAX_VERSION_3_2` value, and the guest feature enum already
  has `GLES_MAX_VERSION_3_2`.
- `host/gl/OpenGLESDispatch/gles32_only.entries` contains six GLES 3.2
  debug-label/group/message functions. The 3.1 dispatch entries and GLES 3.x
  extension entries cover other APIs promoted into GLES 3.2, but that alone
  does not prove the translator implements every required GLES 3.2 behavior.

Therefore, adding the 3.2 feature string or changing the reported version alone
would be incorrect: it could expose APIs the host translator cannot actually
execute.

## Required implementation before enabling 3.2

1. Build from the matching Android Emulator `external/qemu` parent tree and
   its pinned `hardware/google/gfxstream` dependency, rather than the
   standalone Gfxstream project. The emulator integration is in
   `external/qemu/android/android-emu/android/opengles.cpp`; it is compiled
   into the backend build and supplies the renderer lifecycle glue.
2. Preserve that integration, including its renderer pointer handoff and
   address-space/snapshot callbacks. The loader in the same file requires
   `android_setOpenglesRenderer` and `android_stopOpenglesRenderer`.
3. Compare the resulting PE exports against the installed Emulator 37.2.12
   DLL, not just the two known missing symbols.
4. Verify the translator's GLES 3.2 API/behavior coverage. Existing 3.1 and
   extension dispatch entries must not be assumed to guarantee full 3.2.
5. Add 3.2 context creation, host max-version detection, feature negotiation,
   and GL version-string handling together.
6. Run translator/dispatch tests and start the exact installed emulator with
   the rebuilt DLL before treating the artifact as usable.

## Safety

Keep the original `libgfxstream_backend.dll` and its backup unchanged until
the replacement passes export comparison and runtime startup tests. Never
advertise GLES 3.2 by changing only a string, extension token, or guest
property.

## Source references

- Emulator renderer integration and required exports:
  `https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/android-emu/android/opengles.cpp`
- Gfxstream build guidance: use the Android emulator tree's
  `external/qemu` build driver for emulator validation, not the standalone
  CMake DLL as a replacement.


## Original Emulator 37.2.12 DLL inspection (2026-10-30)

The user supplied the untouched DLL at `orig_dll/libgfxstream_backend.dll`.
It was inspected with PE parsing in a Windows GitHub Actions runner.

- File size: 22,858,904 bytes
- SHA-256: `6cd0f21087fa6e6c4f5ab69de59bfe6d5d0cf663ff59b2517796d4d0886545d1`
- PE machine: AMD64 / x86-64 (`0x8664`)
- PE timestamp: 2026-09-23 20:54:26 UTC
- Named exports: 349

A complete name-by-name comparison with the previous standalone build
(345 named exports) proves it is missing exactly four names from the original:

- `AmdPowerXpressRequestHighPerformance`
- `NvOptimusEnablement`
- `android_setOpenglesRenderer`
- `android_stopOpenglesRenderer`

There are no extra named exports in the standalone build. The two
Amd/Nv exports are GPU-selection hints; the two android exports are required
emulator renderer lifecycle entry points. All four must be retained in a
replacement build. This confirms that adding only two stub exports would
still not reproduce the original DLL's export surface.

The original DLL also imports emulator integration libraries including
`libandroid-emu-base-logging.dll`, `libandroid-emu-agents.dll`,
`libandroid-emu-protos.dll`, `libandroid-emu-metrics.dll`,
`libabseil_dll.dll`, and `liblibprotobuf.dll`. This further confirms the
need to build through the emulator parent integration rather than the
standalone Gfxstream target.

The complete machine-readable PE report and export list are available as the
GitHub Actions artifact named `original-dll-analysis` from run
`41006969411`.


## GLES 3.2 negotiation patch (2026-10-30)

A guarded host-side negotiation patch is being prepared on branch
`gles32-negotiation`:

- The host capability probe now attempts GLES 3.2 before 3.1 and 3.0.
- If the EGL backend provides `eglGetMaxGLESVersion`, that backend-reported
  maximum remains authoritative; the host does not override it.
- RenderControl now maps a confirmed 3.2 maximum to the already-supported
  guest token `ANDROID_EMU_gles_max_version_3_2` and reports `OpenGL ES 3.2`
  in the GL version string.
- The Gfxstream `EmulationGl` path in this checkout already allows a maximum
  dispatch version of 3.2, and the shared enums and guest feature parser
  already define 3.2.

A Windows compile-only workflow was added for this branch. It deliberately
does not upload or label its standalone DLL as an emulator replacement.
Passing that workflow would establish only that the changed Gfxstream source
compiles; it would not establish parent-emulator integration, the original
349-export ABI, or runtime GLES 3.2 support.

**Not complete / not installable yet:** the exact source revision for
Emulator 37.2.12 (BuildId 16428233) remains unidentified. The translator's
actual GLES 3.2 execution coverage has not yet been demonstrated on the
NVIDIA GTX 1650 / Windows host. The parent `external/qemu` + pinned
`hardware/google/gfxstream` build, PE export comparison, and exact 37.2.12
runtime test are still required. Do not install a DLL from the standalone
compile workflow.
