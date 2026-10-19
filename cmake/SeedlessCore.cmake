if(NOT TARGET recon_fn)
    message(FATAL_ERROR "SeedlessCore.cmake requires the recon_fn target")
endif()
set(_core "${SEEDLESS_CORE_SOURCE}/src")

target_include_directories(recon_fn PUBLIC "${SEEDLESS_CORE_SOURCE}/include" PRIVATE "${_core}" "${_core}/..")

target_sources(recon_fn PRIVATE
    "${_core}/dma.c"
    "${_core}/ipc.c"
    "${_core}/math.c"
    "${_core}/rtc.c"
    "${_core}/timers.c"
)

# Mirror the pinned upstream module lists while deliberately omitting
# src/platform/android/CMakeLists.txt and selecting POSIX file translation.
foreach(module IN ITEMS
        cart core cpu cpu/jit-arm64 debug frontend frontend/archive
        frontend/cheats frontend/script gpu gpu/gpu2d gpu/gpu3d
        gpu/gpu3d/raster memory platform spi spu util)
    set(module_file "${_core}/${module}/CMakeLists.txt")
    if(NOT EXISTS "${module_file}")
        message(FATAL_ERROR "Pinned core module list is missing: ${module_file}")
    endif()
    include("${module_file}")
endforeach()

set(_posix_files "${_core}/platform/posix/files_posix.c")
if(NOT EXISTS "${_posix_files}")
    message(FATAL_ERROR "Pinned core POSIX file adapter is missing: ${_posix_files}")
endif()
target_sources(recon_fn PRIVATE "${_posix_files}")

# The runner retains emulated SPU and software screen buffers but excludes device
# presentation/capture backends from the Android frontend module lists.
get_target_property(_core_sources recon_fn SOURCES)
foreach(_excluded IN ITEMS
        "${_core}/frontend/audio_out.c"
        "${_core}/frontend/mic.c"
        "${_core}/frontend/shaders.c"
        "${_core}/frontend/video_out_gl.c"
        "${_core}/core/nds_api_fx.c")
    list(REMOVE_ITEM _core_sources "${_excluded}")
endforeach()
set_property(TARGET recon_fn PROPERTY SOURCES "${_core_sources}")
target_sources(recon_fn PRIVATE
    "${CMAKE_SOURCE_DIR}/src/platform/linux/headless_audio.c"
    "${CMAKE_SOURCE_DIR}/src/platform/linux/software_video.c"
    "${CMAKE_SOURCE_DIR}/src/platform/linux/log.c"
    "${CMAKE_SOURCE_DIR}/src/platform/linux/memory.c")

add_library(seedless_abi_probe OBJECT "${CMAKE_SOURCE_DIR}/tests/abi/core_layout.c")
target_include_directories(seedless_abi_probe PRIVATE "${_core}" "${_core}/..")
target_compile_definitions(seedless_abi_probe PRIVATE _GNU_SOURCE)

target_compile_definitions(recon_fn PRIVATE _GNU_SOURCE)
target_compile_options(recon_fn PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(recon_fn PRIVATE
    seedless_lua seedless_libarchive seedless_unarr seedless_lzma
    ZLIB::ZLIB Threads::Threads ${CMAKE_DL_LIBS} "${SEEDLESS_STATIC_LIBM}")
set_target_properties(recon_fn PROPERTIES POSITION_INDEPENDENT_CODE ON)

