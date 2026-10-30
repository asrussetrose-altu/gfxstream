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
- `host/gl/OpenGLESDispatch/gles32_only.entries` contains only the six
  GLES 3.2 debug-label/group/message functions. This is not evidence of a
  complete GLES 3.2 implementation or dispatch table.

Therefore, adding the 3.2 feature string or changing the reported version alone
would be incorrect: it could expose APIs the host translator cannot actually
execute.

## Required implementation before enabling 3.2

1. Build from the matching Android Emulator `external/qemu` parent tree and
   its pinned `hardware/google/gfxstream` dependency, rather than the
   standalone Gfxstream project.
2. Preserve the complete emulator integration and compare the resulting PE
   exports against the installed Emulator 37.2.12 DLL.
3. Extend the host GLES dispatch/translator with the complete GLES 3.2 core
   functionality required by the advertised version, not just debug APIs.
4. Add 3.2 context creation, host max-version detection, feature negotiation,
   and GL version-string handling together.
5. Run translator/dispatch tests and start the exact installed emulator with
   the rebuilt DLL before treating the artifact as usable.

## Safety

Keep the original `libgfxstream_backend.dll` and its backup unchanged until
the replacement passes export comparison and runtime startup tests. Never
advertise GLES 3.2 by changing only a string, extension token, or guest
property.
