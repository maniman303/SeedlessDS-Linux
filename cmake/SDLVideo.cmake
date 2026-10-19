find_package(PkgConfig REQUIRED)

if(CMAKE_CROSSCOMPILING)
    if(CMAKE_SYSROOT)
        set(_seedless_sdl_sysroot "${CMAKE_SYSROOT}")
    elseif(CMAKE_FIND_ROOT_PATH)
        list(GET CMAKE_FIND_ROOT_PATH 0 _seedless_sdl_sysroot)
    else()
        message(FATAL_ERROR "Cross SDL2 discovery requires an explicit target sysroot")
    endif()
    if(NOT IS_DIRECTORY "${_seedless_sdl_sysroot}")
        message(FATAL_ERROR "SDL2 target sysroot does not exist: ${_seedless_sdl_sysroot}")
    endif()
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${_seedless_sdl_sysroot}")
    set(ENV{PKG_CONFIG_LIBDIR}
        "${_seedless_sdl_sysroot}/usr/lib/aarch64-linux-gnu/pkgconfig:${_seedless_sdl_sysroot}/usr/lib/pkgconfig:${_seedless_sdl_sysroot}/usr/share/pkgconfig:${_seedless_sdl_sysroot}/lib/aarch64-linux-gnu/pkgconfig")
endif()

pkg_check_modules(SEEDLESS_SDL2 REQUIRED IMPORTED_TARGET GLOBAL sdl2>=2.0.12)

# SDL2 2.0.12 is common on older target distributions that do not ship an
# SDL2Config.cmake file. PkgConfig's imported target observes the active CMake
# sysroot and provides the installed shared SDL2 link interface without SDL2main.
if(NOT TARGET PkgConfig::SEEDLESS_SDL2)
    message(FATAL_ERROR "pkg-config did not provide the installed SDL2 target")
endif()
add_library(SDL2::SDL2 ALIAS PkgConfig::SEEDLESS_SDL2)
unset(_seedless_sdl_sysroot)
