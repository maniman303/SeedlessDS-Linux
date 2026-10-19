find_package(PkgConfig REQUIRED)

if(CMAKE_CROSSCOMPILING)
    set(_seedless_sdl_sysroot "")
    if(DEFINED ENV{SEEDLESS_TARGET_SYSROOT} AND NOT "$ENV{SEEDLESS_TARGET_SYSROOT}" STREQUAL "")
        set(_seedless_sdl_sysroot "$ENV{SEEDLESS_TARGET_SYSROOT}")
    elseif(CMAKE_SYSROOT)
        set(_seedless_sdl_sysroot "${CMAKE_SYSROOT}")
    endif()
    if(_seedless_sdl_sysroot)
        if(NOT IS_DIRECTORY "${_seedless_sdl_sysroot}")
            message(FATAL_ERROR "SDL2 target sysroot does not exist: ${_seedless_sdl_sysroot}")
        endif()
        set(ENV{PKG_CONFIG_SYSROOT_DIR} "${_seedless_sdl_sysroot}")
        set(ENV{PKG_CONFIG_LIBDIR}
            "${_seedless_sdl_sysroot}/usr/lib/aarch64-linux-gnu/pkgconfig:${_seedless_sdl_sysroot}/usr/lib/pkgconfig:${_seedless_sdl_sysroot}/usr/share/pkgconfig:${_seedless_sdl_sysroot}/lib/aarch64-linux-gnu/pkgconfig")
    else()
        # Debian/Ubuntu cross packages install target libraries in the host's
        # multiarch tree, outside the compiler's libc sysroot. pkg-config must
        # search only that target tree and must not prefix it with /usr/aarch64-linux-gnu.
        set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
        set(ENV{PKG_CONFIG_LIBDIR}
            "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
    endif()
    set(ENV{PKG_CONFIG_PATH} "")
endif()

pkg_check_modules(SEEDLESS_SDL2 REQUIRED IMPORTED_TARGET GLOBAL
    NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH sdl2>=2.0.12)

# SDL2 2.0.12 is common on older target distributions that do not ship an
# SDL2Config.cmake file. The imported target supplies the selected target
# package's shared SDL2 link interface without SDL2main.
if(NOT TARGET PkgConfig::SEEDLESS_SDL2)
    message(FATAL_ERROR "pkg-config did not provide the installed SDL2 target")
endif()
add_library(SDL2::SDL2 ALIAS PkgConfig::SEEDLESS_SDL2)
unset(_seedless_sdl_sysroot)
