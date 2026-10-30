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


## Translator source audit (2026-10-30)

A source-level audit of the translator shows why the negotiation patch alone
cannot enable GLES 3.2. The checkout contains partial 3.2 scaffolding, but
the version plumbing is not connected end-to-end:

- `EglOS::GlesVersion::ES32` exists in `egl_os_api.h`, and
  `gles32_only.entries` plus generated dispatch headers/implementation exist.
  These are partial scaffolding, not proof of complete GLES 3.2 support.
- The translator's `GLESVersion` enum in
  `host/gl/glestranslator/common/include/common/gl_utils.h` stops at
  `GLES_3_1`.
- `GLDispatch::clearDispatchFuncs()` and `dispatchFuncs()` clear/load
  GLES 3.1 functions but do not load the GLES 3.2-only dispatch list.
- `egl_imp.cpp` registers translator interfaces only through
  `GLES_3_1`; its EGL context-version switch rejects GLES 3.2 (minor 2).
- `eglSetMaxGLESVersion()` explicitly maps the host value 3 (GLES 3.2)
  back to `GLES_3_1`.
- `egl_os_api_egl.cpp` probes a 3.1 context and then 3.0; it does not
  probe a 3.2 context despite the existing `ES32` enum.
- `egl_display.cpp` has no explicit GLES 3.2 object-manager alias.
- `GLESv2Context` already has a 3.2 version-string case, but its capability
  selection currently has a dedicated 3.1 capability structure only.

The required translator work is therefore larger than the host negotiation
change: connect the 3.2 version through EGL interface registration, context
validation, host context probing, dispatch loading/clearing, capability
reporting, and feature/extension filtering. Then run GLES 3.2 CTS/dEQP tests
to establish behavior. Until that work is complete, the PR must remain
non-installable and must not advertise 3.2 as supported.


## Follow-up translator audit: version tables and dispatch (2026-10-30)

Additional checks confirmed that the GLES 3.2 gap is not caused by missing
generated headers. The repository already contains
`gles32_only.entries`, its generated function-list header, and generated
logging wrappers. The GLESv2 dispatch header includes the generated 3.2
namespaced declarations.

The missing runtime wiring is explicit:

- `GLESVersion` currently ends at `GLES_3_1`; the interface arrays in
  `EglGlobalInfo` are sized by `MAX_GLES_VERSION`, so adding a new enum
  value must be accompanied by reviewing all version-indexed tables.
- `GLDispatch::clearDispatchFuncs()` clears GLES 3.1-only functions but not
  GLES 3.2-only functions.
- `GLDispatch::dispatchFuncs()` loads GLES 3.1-only functions but does not
  load the six GLES 3.2-only functions, even though their generated list is
  available.
- EGL startup does not register a GLES 3.2 translator interface.
- EGL context creation rejects minor version 2.
- The EGL max-version setter maps reported value 3 (GLES 3.2) back to 3.1.
- The native EGL probe attempts 3.1 and 3.0 only; the enum's ES32 value is
  currently unused by that probe.
- The versioned extension/capability caches and vendor/renderer/version
  strings have special handling for 3.1 only. A 3.2 context must not
  accidentally reuse the generic 3.0 cache or expose the wrong extension set.
- `calcMaxESVersionFromCoreVersion()` currently caps desktop GL 4.x at
  ES 3.1. Any extension of that mapping needs to be based on the actual
  host API/translator requirements, not just a numeric GL version.

This narrows the implementation plan, but does not make the change safe to
enable yet. Before changing runtime negotiation, the translator needs a
consistent 3.2 interface, context, dispatch, cache, and capability path, and
the exact host backend must pass the GLES 3.2 CTS/dEQP coverage relevant to
the features it advertises.


## GLES 3.2 API-surface coverage check (2026-10-30)

The generated `gles32_only.entries` list is only six KHR_debug-style
functions: debug message control/insert/callback/log and debug group push/pop.
This explains why generated 3.2 headers are present despite the translator
being capped at 3.1.

That six-function list is not the complete GLES 3.2 feature set. GLES 3.2
also makes additional functionality core that historically arrived through
extensions, notably geometry shaders and tessellation shaders, as well as
other promoted functionality. The inspected GLES 3.0, 3.1 and 3.x extension
entry lists do not show a complete, explicit geometry/tessellation API
dispatch implementation. Therefore, we cannot infer GLES 3.2 conformance
from the presence of the six generated debug functions. A proper coverage
matrix must map every GLES 3.2 core feature to either a translator core
implementation or a tested extension implementation before version 3.2 is
advertised.


## Specification-level coverage checklist

The Khronos GLES 3.2 specification's "New Features" section includes more
than the six debug functions in the generated 3.2-only dispatch list. The
implementation review must explicitly cover:

- Geometry and tessellation shader stages.
- Shader image atomic operations and shader interface blocks.
- Multisample interpolation, sample shading, and sample variables.
- Texture buffers, texture border color, cube-map arrays, and multisample
  2D texture arrays.
- STENCIL8 texture formats.
- Base-vertex draw calls and robust buffer access/context flag behavior.
- The Android extension-pack functionality incorporated into GLES 3.2,
  including ASTC LDR, floating-point render targets, advanced blending, and
  per-draw-buffer blending.

Some of these may already be implemented through the GLES 3.1 core path or
extension translation. They are marked **unverified**, not unsupported,
until each feature is traced to concrete translator code and exercised by
the corresponding CTS/dEQP tests.

Reference: Khronos, OpenGL ES 3.2 Specification,
https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf


## Windows NVIDIA fast-path investigation

The Windows translator path uses WGL. Its current implementation creates a
desktop **core-profile** context, records the highest core version, and passes
that version to `calcMaxESVersionFromCoreVersion()`. The helper intentionally
caps OpenGL 4.x at GLES 3.1.

There is a potentially useful NVIDIA-specific alternative worth testing before
implementing a large shader translator:

- `GL_ARB_ES3_2_compatibility` adds GLES 3.2 features missing from desktop
  OpenGL 4.5, but the Khronos extension specification is written against the
  OpenGL 4.5 **compatibility profile**.
- It does not establish that the current WGL core-profile context exposes the
  extension or that Gfxstream can use it safely.
- The GTX 1650 / NVIDIA 592.82 machine's actual extension list has not yet
  been captured.

A read-only Windows diagnostic utility has therefore been added at
`tools/windows-gl-capability-probe.cpp`. It creates temporary hidden-window
OpenGL 4.5 core and compatibility contexts and reports whether
`GL_ARB_ES3_2_compatibility`, `GL_ARB_ES3_1_compatibility`, and related
feature extensions are exposed by the local driver. It does not modify the
emulator, driver, registry, or AVDs.

The workflow now has a separate job to compile and upload this diagnostic
utility as `windows-gl-capability-probe.exe`. This is a diagnostic only, not
a replacement DLL. If the extension is present only in compatibility mode,
we must still validate whether Gfxstream can use that context profile without
breaking its rendering assumptions.
