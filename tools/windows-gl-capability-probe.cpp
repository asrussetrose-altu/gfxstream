// Read-only WGL/OpenGL capability probe for Windows.
// It creates temporary hidden-window contexts and does not modify the driver,
// emulator installation, registry, or AVD files.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif

#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef const char* (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC);
typedef const GLubyte* (APIENTRY * PFNGLGETSTRINGIPROC)(GLenum, GLuint);

static bool validProc(PROC p) {
    return p && p != (PROC)1 && p != (PROC)2 && p != (PROC)3 && p != (PROC)-1;
}

static bool hasExtension(PFNGLGETSTRINGIPROC getStringi, int count, const char* wanted) {
    for (int i = 0; i < count; ++i) {
        const char* value = reinterpret_cast<const char*>(getStringi(GL_EXTENSIONS, (GLuint)i));
        if (value && std::strcmp(value, wanted) == 0) return true;
    }
    return false;
}

static void inspectContext(const char* label, HDC dc, HGLRC context) {
    if (!wglMakeCurrent(dc, context)) {
        std::printf("%s: wglMakeCurrent failed, error=%lu\n", label, GetLastError());
        return;
    }

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    std::printf("\n[%s]\n", label);
    std::printf("Vendor: %s\n", vendor ? vendor : "(null)");
    std::printf("Renderer: %s\n", renderer ? renderer : "(null)");
    std::printf("OpenGL: %s\n", version ? version : "(null)");
    std::printf("GLSL: %s\n", glsl ? glsl : "(null)");

    auto getStringi = reinterpret_cast<PFNGLGETSTRINGIPROC>(wglGetProcAddress("glGetStringi"));
    if (!validProc(reinterpret_cast<PROC>(getStringi))) {
        std::printf("glGetStringi unavailable; cannot enumerate core-profile extensions.\n");
        return;
    }

    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    std::printf("Extension count: %d\n", count);

    const char* checks[] = {
        "GL_ARB_ES3_2_compatibility",
        "GL_ARB_ES3_1_compatibility",
        "GL_KHR_blend_equation_advanced",
        "GL_KHR_texture_compression_astc_ldr",
        "GL_ARB_geometry_shader4",
        "GL_ARB_tessellation_shader",
        "GL_ARB_texture_cube_map_array",
        "GL_ARB_texture_buffer_object",
        "GL_ARB_texture_multisample",
        "GL_ARB_robustness",
        "GL_KHR_robustness"
    };
    for (const char* ext : checks) {
        std::printf("%s: %s\n", ext,
                    hasExtension(getStringi, count, ext) ? "YES" : "NO");
    }
}

int main() {
    std::printf("Windows OpenGL/WGL capability probe (read-only)\n");
    WNDCLASSA wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "GfxstreamGlProbeWindow";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        std::printf("RegisterClass failed: %lu\n", GetLastError());
        return 1;
    }

    HWND window = CreateWindowA(wc.lpszClassName, "GL probe", WS_POPUP,
                                0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    if (!window) {
        std::printf("CreateWindow failed: %lu\n", GetLastError());
        return 1;
    }
    HDC dc = GetDC(window);
    if (!dc) {
        std::printf("GetDC failed: %lu\n", GetLastError());
        DestroyWindow(window);
        return 1;
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pixelFormat = ChoosePixelFormat(dc, &pfd);
    if (!pixelFormat || !SetPixelFormat(dc, pixelFormat, &pfd)) {
        std::printf("Pixel format setup failed: %lu\n", GetLastError());
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return 1;
    }

    HGLRC legacy = wglCreateContext(dc);
    if (!legacy || !wglMakeCurrent(dc, legacy)) {
        std::printf("Legacy WGL context failed: %lu\n", GetLastError());
        if (legacy) wglDeleteContext(legacy);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return 1;
    }

    auto createAttribs = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
        wglGetProcAddress("wglCreateContextAttribsARB"));
    auto getWglExts = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(
        wglGetProcAddress("wglGetExtensionsStringARB"));
    if (validProc(reinterpret_cast<PROC>(getWglExts))) {
        const char* exts = getWglExts(dc);
        std::printf("WGL extensions: %s\n", exts ? exts : "(null)");
    }
    if (!validProc(reinterpret_cast<PROC>(createAttribs))) {
        std::printf("wglCreateContextAttribsARB unavailable; only legacy context was created.\n");
    } else {
        const int profiles[] = {
            WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
        };
        const char* names[] = {"OpenGL 4.5 CORE", "OpenGL 4.5 COMPATIBILITY"};
        for (int i = 0; i < 2; ++i) {
            const int attrs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                WGL_CONTEXT_MINOR_VERSION_ARB, 5,
                WGL_CONTEXT_PROFILE_MASK_ARB, profiles[i],
                0
            };
            HGLRC ctx = createAttribs(dc, nullptr, attrs);
            if (!ctx) {
                std::printf("\n[%s] context creation failed, error=%lu\n", names[i], GetLastError());
                continue;
            }
            inspectContext(names[i], dc, ctx);
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(ctx);
        }
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacy);
    ReleaseDC(window, dc);
    DestroyWindow(window);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    std::printf("\nProbe complete. No emulator files or settings were changed.\n");
    return 0;
}
