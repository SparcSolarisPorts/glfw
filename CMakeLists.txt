cmake_minimum_required(VERSION 3.16...3.28 FATAL_ERROR)

project(GLFW VERSION 3.6.0 LANGUAGES C HOMEPAGE_URL "https://www.glfw.org/")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

string(COMPARE EQUAL "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}" GLFW_STANDALONE)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" ${GLFW_STANDALONE})
option(GLFW_BUILD_TESTS "Build the GLFW test programs" ${GLFW_STANDALONE})
option(GLFW_BUILD_DOCS "Build the GLFW documentation" ON)
option(GLFW_INSTALL "Generate installation target" ON)

include(GNUInstallDirs)
include(CMakeDependentOption)

if (GLFW_USE_OSMESA)
    message(FATAL_ERROR "GLFW_USE_OSMESA has been removed; set the GLFW_PLATFORM init hint")
endif()

if (DEFINED GLFW_USE_WAYLAND AND UNIX AND NOT APPLE)
    message(FATAL_ERROR
        "GLFW_USE_WAYLAND has been removed; delete the CMake cache and set GLFW_BUILD_WAYLAND and GLFW_BUILD_X11 instead")
endif()

cmake_dependent_option(GLFW_BUILD_WIN32 "Build support for Win32" ON "WIN32" OFF)
cmake_dependent_option(GLFW_BUILD_COCOA "Build support for Cocoa" ON "APPLE" OFF)
cmake_dependent_option(GLFW_BUILD_X11 "Build support for X11" ON "UNIX;NOT APPLE" OFF)
# Solaris and illumos both identify as SunOS.  X11 is the established
# desktop backend there; Wayland remains available as an explicit opt-in.
if (CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(GLFW_WAYLAND_DEFAULT OFF)
else()
    set(GLFW_WAYLAND_DEFAULT ON)
endif()
cmake_dependent_option(GLFW_BUILD_WAYLAND "Build support for Wayland" ${GLFW_WAYLAND_DEFAULT} "UNIX;NOT APPLE" OFF)
unset(GLFW_WAYLAND_DEFAULT)

cmake_dependent_option(GLFW_USE_HYBRID_HPG "Force use of high-performance GPU on hybrid systems" OFF
                       "WIN32" OFF)
cmake_dependent_option(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC runtime library DLL" ON
                       "MSVC" OFF)

set(GLFW_LIBRARY_TYPE "${GLFW_LIBRARY_TYPE}" CACHE STRING
    "Library type override for GLFW (SHARED, STATIC, OBJECT, or empty to follow BUILD_SHARED_LIBS)")

if (GLFW_LIBRARY_TYPE)
    string(COMPARE EQUAL "${GLFW_LIBRARY_TYPE}" "SHARED" GLFW_BUILD_SHARED_LIBRARY)
else()
    set(GLFW_BUILD_SHARED_LIBRARY ${BUILD_SHARED_LIBS})
endif()

list(APPEND CMAKE_MODULE_PATH "${GLFW_SOURCE_DIR}/CMake/modules")

# SunOS uses ISA-specific 64-bit library directories (sparcv9, amd64, etc.).
# Probe the running build host instead of treating SunOS like Linux.  Solaris
# and illumos share this ABI convention.
if (CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(_GLFW_SUNOS_ISA "")

    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        find_program(_GLFW_SUNOS_ISAINFO NAMES isainfo PATHS /usr/bin /bin)
        if (_GLFW_SUNOS_ISAINFO)
            execute_process(COMMAND "${_GLFW_SUNOS_ISAINFO}" -k
                            OUTPUT_VARIABLE _GLFW_SUNOS_ISA
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            ERROR_QUIET)
        endif()

        if (NOT _GLFW_SUNOS_ISA)
            string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _GLFW_SUNOS_PROCESSOR)
            if (_GLFW_SUNOS_PROCESSOR MATCHES "sparcv9|sparc64|sparc")
                set(_GLFW_SUNOS_ISA "sparcv9")
            elseif (_GLFW_SUNOS_PROCESSOR MATCHES "amd64|x86_64")
                set(_GLFW_SUNOS_ISA "amd64")
            elseif (_GLFW_SUNOS_PROCESSOR MATCHES "aarch64|arm64")
                foreach(_candidate aarch64 arm64 64)
                    if (IS_DIRECTORY "/usr/lib/${_candidate}")
                        set(_GLFW_SUNOS_ISA "${_candidate}")
                        break()
                    endif()
                endforeach()
            endif()
        endif()
    endif()

    # Keep 64-bit ISA directories ahead of unsuffixed /usr/lib.  Solaris
    # packages 32-bit objects in /usr/lib and 64-bit objects in /usr/lib/amd64
    # or /usr/lib/sparcv9, so reversing this order can make CMake inspect the
    # wrong ELF class first.
    set(_GLFW_SUNOS_LIBRARY_DIRS "")
    set(_GLFW_SUNOS_PKGCONFIG_DIRS "")

    if (_GLFW_SUNOS_ISA)
        list(APPEND _GLFW_SUNOS_LIBRARY_DIRS
             "/usr/lib/${_GLFW_SUNOS_ISA}"
             "/usr/lib/GL/${_GLFW_SUNOS_ISA}"
             "/usr/lib/mesa/${_GLFW_SUNOS_ISA}"
             "/usr/X11/lib/${_GLFW_SUNOS_ISA}"
             "/usr/openwin/lib/${_GLFW_SUNOS_ISA}")
        list(APPEND _GLFW_SUNOS_PKGCONFIG_DIRS
             "/usr/lib/${_GLFW_SUNOS_ISA}/pkgconfig")
    endif()

    list(APPEND _GLFW_SUNOS_LIBRARY_DIRS
         /usr/lib
         /usr/lib/GL
         /usr/lib/mesa
         /usr/lib/X11
         /usr/X11/lib
         /usr/openwin/lib)
    list(APPEND _GLFW_SUNOS_PKGCONFIG_DIRS /usr/lib/pkgconfig)

    set(_GLFW_SUNOS_FOUND_LIBRARY_DIRS "")
    foreach(_dir IN LISTS _GLFW_SUNOS_LIBRARY_DIRS)
        if (IS_DIRECTORY "${_dir}")
            list(APPEND _GLFW_SUNOS_FOUND_LIBRARY_DIRS "${_dir}")
        endif()
    endforeach()
    if (_GLFW_SUNOS_FOUND_LIBRARY_DIRS)
        # Prepend the complete ordered list once.  Repeated PREPEND operations
        # would reverse the order and put 32-bit /usr/lib ahead of sparcv9.
        list(PREPEND CMAKE_LIBRARY_PATH ${_GLFW_SUNOS_FOUND_LIBRARY_DIRS})
    endif()

    # X11 headers are in /usr/include/X11.  Mesa's implementation headers are
    # in /usr/include/mesa with selected public GL/EGL links under /usr/include.
    set(_GLFW_SUNOS_FOUND_INCLUDE_DIRS "")
    foreach(_dir /usr/include /usr/include/mesa /usr/include/X11)
        if (IS_DIRECTORY "${_dir}")
            list(APPEND _GLFW_SUNOS_FOUND_INCLUDE_DIRS "${_dir}")
        endif()
    endforeach()
    if (_GLFW_SUNOS_FOUND_INCLUDE_DIRS)
        list(PREPEND CMAKE_INCLUDE_PATH ${_GLFW_SUNOS_FOUND_INCLUDE_DIRS})
    endif()

    set(_GLFW_SUNOS_PC_PATHS "")
    foreach(_dir IN LISTS _GLFW_SUNOS_PKGCONFIG_DIRS)
        if (IS_DIRECTORY "${_dir}")
            list(APPEND _GLFW_SUNOS_PC_PATHS "${_dir}")
        endif()
    endforeach()
    if (_GLFW_SUNOS_PC_PATHS)
        string(JOIN ":" _GLFW_SUNOS_PC_PATH ${_GLFW_SUNOS_PC_PATHS})
        if (DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
            set(ENV{PKG_CONFIG_PATH} "${_GLFW_SUNOS_PC_PATH}:$ENV{PKG_CONFIG_PATH}")
        else()
            set(ENV{PKG_CONFIG_PATH} "${_GLFW_SUNOS_PC_PATH}")
        endif()
    endif()

    message(STATUS "SunOS ISA library suffix: ${_GLFW_SUNOS_ISA}")
endif()

find_package(Threads REQUIRED)

#--------------------------------------------------------------------
# Report backend selection
#--------------------------------------------------------------------
if (GLFW_BUILD_WIN32)
    message(STATUS "Including Win32 support")
endif()
if (GLFW_BUILD_COCOA)
    message(STATUS "Including Cocoa support")
endif()
if (GLFW_BUILD_WAYLAND)
    message(STATUS "Including Wayland support")
endif()
if (GLFW_BUILD_X11)
    message(STATUS "Including X11 support")
endif()

#--------------------------------------------------------------------
# Apply Microsoft C runtime library option
# This is here because it also applies to tests and examples
#--------------------------------------------------------------------
if (MSVC AND NOT USE_MSVC_RUNTIME_LIBRARY_DLL)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

#--------------------------------------------------------------------
# Create generated files
#--------------------------------------------------------------------
include(CMakePackageConfigHelpers)

set(GLFW_CONFIG_PATH "${CMAKE_INSTALL_LIBDIR}/cmake/glfw3")

configure_package_config_file(CMake/glfw3Config.cmake.in
                              src/glfw3Config.cmake
                              INSTALL_DESTINATION "${GLFW_CONFIG_PATH}"
                              NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(src/glfw3ConfigVersion.cmake
                                 VERSION ${GLFW_VERSION}
                                 COMPATIBILITY SameMajorVersion)

#--------------------------------------------------------------------
# Add subdirectories
#--------------------------------------------------------------------
add_subdirectory(src)

if (GLFW_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (GLFW_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (GLFW_BUILD_DOCS)
    add_subdirectory(docs)
endif()

#--------------------------------------------------------------------
# Install files other than the library
# The library is installed by src/CMakeLists.txt
#--------------------------------------------------------------------
if (GLFW_INSTALL)
    install(DIRECTORY include/GLFW DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN glfw3.h PATTERN glfw3native.h)

    install(FILES "${GLFW_BINARY_DIR}/src/glfw3Config.cmake"
                  "${GLFW_BINARY_DIR}/src/glfw3ConfigVersion.cmake"
            DESTINATION "${GLFW_CONFIG_PATH}")

    install(EXPORT glfwTargets FILE glfw3Targets.cmake
            EXPORT_LINK_INTERFACE_LIBRARIES
            DESTINATION "${GLFW_CONFIG_PATH}")
    install(FILES "${GLFW_BINARY_DIR}/src/glfw3.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # Only generate this target if no higher-level project already has
    if (NOT TARGET uninstall)
        configure_file(CMake/cmake_uninstall.cmake.in
                       cmake_uninstall.cmake IMMEDIATE @ONLY)

        add_custom_target(uninstall
                          "${CMAKE_COMMAND}" -P
                          "${GLFW_BINARY_DIR}/cmake_uninstall.cmake")
        set_target_properties(uninstall PROPERTIES FOLDER "GLFW3")
    endif()
endif()

