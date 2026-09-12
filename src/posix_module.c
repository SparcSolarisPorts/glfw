//========================================================================
// GLFW 3.6 POSIX - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2021 Camilla Löwy <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"

#if defined(GLFW_BUILD_POSIX_MODULE)

#include <dlfcn.h>

#if defined(__sun)
#include <stdio.h>
#include <string.h>
#include <sys/systeminfo.h>
#endif

//////////////////////////////////////////////////////////////////////////
//////                       GLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

#if defined(__sun)
static void* _glfwTryModuleSunOS(const char* path)
{
    void* module;
    char isa[64] = {0};
    char candidate[1024];

    // Always ask the runtime linker first.  This preserves crle(1), RUNPATH,
    // LD_LIBRARY_PATH and Solaris service/opengl/ogl-select selection.
    module = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (module || strchr(path, '/'))
        return module;

    // 64-bit Solaris and illumos keep ELF objects in ISA subdirectories.
    // On Oracle Solaris these are normally sparcv9 and amd64.  Query the
    // kernel rather than inferring the ISA from compiler predefined macros.
    if (sizeof(void*) == 8 &&
        sysinfo(SI_ARCHITECTURE_64, isa, sizeof(isa)) > 0 && isa[0])
    {
        const char* formats[] =
        {
            "/usr/lib/%s/%s",
            "/usr/lib/GL/%s/%s",
            "/usr/lib/mesa/%s/%s",
            "/usr/X11/lib/%s/%s",
            "/usr/openwin/lib/%s/%s",
            NULL
        };

        for (int i = 0; formats[i]; i++)
        {
            const int length = snprintf(candidate, sizeof(candidate), formats[i], isa, path);
            if (length >= 0 && length < (int) sizeof(candidate))
            {
                module = dlopen(candidate, RTLD_LAZY | RTLD_LOCAL);
                if (module)
                    return module;
            }
        }
    }

    // 32-bit objects and compatibility links use the unsuffixed directories.
    {
        const char* directories[] =
        {
            "/usr/lib",
            "/usr/lib/GL",
            "/usr/lib/mesa",
            "/usr/X11/lib",
            "/usr/openwin/lib",
            NULL
        };

        for (int i = 0; directories[i]; i++)
        {
            const int length = snprintf(candidate, sizeof(candidate), "%s/%s", directories[i], path);
            if (length >= 0 && length < (int) sizeof(candidate))
            {
                module = dlopen(candidate, RTLD_LAZY | RTLD_LOCAL);
                if (module)
                    return module;
            }
        }
    }

    return NULL;
}

static void* _glfwLoadModuleSunOS(const char* path)
{
    void* module = _glfwTryModuleSunOS(path);
    if (module)
        return module;

    if (strchr(path, '/'))
        return NULL;

    // GLFW follows Linux ABI SONAMEs for several dlopen'ed X11 libraries.
    // Solaris development packages expose an unversioned libFoo.so link in
    // the native ISA directory, so strip a trailing numeric .so ABI first.
    // This avoids hard-coding Solaris ABI numbers for every X11 extension.
    {
        const char* so = strstr(path, ".so.");
        if (so && so[4])
        {
            const char* version = so + 4;
            GLFWbool numeric = GLFW_TRUE;
            for (const char* p = version; *p; p++)
            {
                if ((*p < '0' || *p > '9') && *p != '.')
                {
                    numeric = GLFW_FALSE;
                    break;
                }
            }

            if (numeric)
            {
                const size_t length = (size_t) (so - path) + 3;
                if (length < 1024)
                {
                    char candidate[1024];
                    memcpy(candidate, path, length);
                    candidate[length] = '\0';
                    module = _glfwTryModuleSunOS(candidate);
                    if (module)
                        return module;
                }
            }
        }
    }

    // libX11 is the important exception where a runtime-only Solaris image
    // may have the ABI library but not an unversioned developer symlink.
    // Solaris 11.4 ships libX11.so.4.0.0 and compatibility .so.4/.so.5 links.
    if (strcmp(path, "libX11.so") == 0 || strcmp(path, "libX11.so.6") == 0)
    {
        static const char* alternatives[] =
        {
            "libX11.so.5",
            "libX11.so.4",
            "libX11.so.4.0.0",
            NULL
        };

        for (int i = 0; alternatives[i]; i++)
        {
            module = _glfwTryModuleSunOS(alternatives[i]);
            if (module)
                return module;
        }
    }

    // Older Solaris X extension ABIs retained these SONAMEs.  Keep them as
    // last-resort compatibility fallbacks after the generic unversioned path.
    if (strcmp(path, "libXext.so") == 0 || strcmp(path, "libXext.so.6") == 0)
        return _glfwTryModuleSunOS("libXext.so.0");
    if (strcmp(path, "libXi.so") == 0 || strcmp(path, "libXi.so.6") == 0)
        return _glfwTryModuleSunOS("libXi.so.5");

    return NULL;
}
#endif

void* _glfwPlatformLoadModule(const char* path)
{
#if defined(__sun)
    return _glfwLoadModuleSunOS(path);
#else
    return dlopen(path, RTLD_LAZY | RTLD_LOCAL);
#endif
}

void _glfwPlatformFreeModule(void* module)
{
    if (module)
        dlclose(module);
}

GLFWproc _glfwPlatformGetModuleSymbol(void* module, const char* name)
{
    return dlsym(module, name);
}

#endif // GLFW_BUILD_POSIX_MODULE

