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
