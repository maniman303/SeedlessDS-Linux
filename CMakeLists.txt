cmake_minimum_required(VERSION 3.28)
project(SeedlessDSLinux VERSION 0.1.0 LANGUAGES C ASM)
include(CTest)
set(SEEDLESS_BUILD_TESTING "${BUILD_TESTING}")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "SeedlessDS-Linux targets Linux only")
endif()
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    message(FATAL_ERROR "SeedlessDS-Linux runtime targets must be ARM64; got ${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(DEFINED CMAKE_C_COMPILER_TARGET AND
   NOT "${CMAKE_C_COMPILER_TARGET}" STREQUAL "" AND
   NOT "${CMAKE_C_COMPILER_TARGET}" MATCHES "^aarch64.*linux")
    message(FATAL_ERROR "Compiler target is not ARM64 Linux: ${CMAKE_C_COMPILER_TARGET}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(SEEDLESS_SDL_BACKEND_RUNTIME_REQUIREMENTS
    "SDL2 target video backend modules and their shared dependencies selected at runtime"
    CACHE STRING "Declared dynamically loaded SDL2 backend runtime requirements")

if(NOT DEFINED SEEDLESS_PRESET OR NOT SEEDLESS_PRESET MATCHES "^linux-arm64-(native|cross)$")
    message(FATAL_ERROR "Set SEEDLESS_PRESET to linux-arm64-native or linux-arm64-cross")
endif()
set(SEEDLESS_CORE_SOURCE "${CMAKE_BINARY_DIR}/core-src" CACHE PATH "Prepared pinned core source")
execute_process(
    COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/prepare-core.sh" "${SEEDLESS_PRESET}"
    RESULT_VARIABLE prepare_result OUTPUT_VARIABLE prepare_output ERROR_VARIABLE prepare_error)
if(NOT prepare_result EQUAL 0)
    message(FATAL_ERROR "Core source preparation failed:\n${prepare_output}${prepare_error}")
endif()
message(STATUS "${prepare_output}")

find_package(Threads REQUIRED)
include("${CMAKE_SOURCE_DIR}/cmake/SDLVideo.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake")

add_library(recon_fn STATIC)
include("${CMAKE_SOURCE_DIR}/cmake/SeedlessCore.cmake")

add_executable(seedlessds
    src/main.c
    src/runner.c
    src/config.c
    src/config_json.c
    src/launch_options.c
    src/core_config.c
    src/video/layout.c
    src/video/frame_mailbox.c
    src/video/sdl_video.c
    src/platform/linux/core_video_bridge.c
    src/platform/linux/platform.c)
target_include_directories(seedlessds PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${SEEDLESS_CORE_SOURCE}/include"
    "${SEEDLESS_CORE_SOURCE}/src")
target_link_libraries(seedlessds PRIVATE recon_fn SDL2::SDL2 Threads::Threads ${CMAKE_DL_LIBS})
target_compile_options(seedlessds PRIVATE -Wall -Wextra -Werror)
target_link_options(seedlessds PRIVATE -static-libgcc -Wl,--no-undefined)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(SEEDLESS_BUILD_TESTING)
    add_executable(seedlessds-video-probe
        tests/video_probe.c
        src/video/layout.c
        src/video/sdl_video.c
        src/config.c
        src/config_json.c
        src/launch_options.c)
    target_include_directories(seedlessds-video-probe PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(seedlessds-video-probe PRIVATE SDL2::SDL2)
    target_compile_definitions(seedlessds-video-probe PRIVATE SEEDLESS_VIDEO_TESTING)
    target_compile_options(seedlessds-video-probe PRIVATE -Wall -Wextra -Werror)
    target_link_options(seedlessds-video-probe PRIVATE -Wl,--no-undefined)
endif()
if(SEEDLESS_PRESET STREQUAL "linux-arm64-cross")
    find_program(SEEDLESS_READELF NAMES aarch64-linux-gnu-readelf REQUIRED)
else()
    find_program(SEEDLESS_READELF NAMES readelf REQUIRED)
endif()
set(_seedless_sdl_backend_audit_args)
foreach(_requirement IN LISTS SEEDLESS_SDL_BACKEND_RUNTIME_REQUIREMENTS)
    list(APPEND _seedless_sdl_backend_audit_args
        --sdl2-backend-requirement "${_requirement}")
endforeach()
add_custom_command(TARGET seedlessds POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/audit-runtime.py"
        --binary "$<TARGET_FILE:seedlessds>"
        --preset "${SEEDLESS_PRESET}"
        --readelf "${SEEDLESS_READELF}"
        --sdl2-version "${SEEDLESS_SDL2_VERSION}"
        ${_seedless_sdl_backend_audit_args}
        --output "${CMAKE_BINARY_DIR}/runtime-inventory.json"
    VERBATIM)

install(TARGETS seedlessds RUNTIME DESTINATION bin)
install(FILES "${CMAKE_SOURCE_DIR}/external/SeedlessDS-Core/LICENSE"
        DESTINATION share/licenses/seedlessds-core RENAME LICENSE)
install(FILES "${CMAKE_BINARY_DIR}/runtime-inventory.json"
        DESTINATION share/seedlessds RENAME runtime-inventory.json)
install(FILES "${CMAKE_SOURCE_DIR}/licenses/Lua.txt"
        DESTINATION share/licenses/seedlessds/dependencies RENAME Lua.txt)
install(FILES "${CMAKE_SOURCE_DIR}/licenses/SDL2.txt"
        DESTINATION share/licenses/seedlessds/dependencies RENAME SDL2.txt)
install(FILES "${CMAKE_SOURCE_DIR}/licenses/LZMA-SDK-9.20.txt"
        DESTINATION share/licenses/seedlessds/dependencies RENAME LZMA-SDK-9.20.txt)
install(FILES "${seedless_zlib_SOURCE_DIR}/LICENSE"
        DESTINATION share/licenses/seedlessds/dependencies RENAME zlib-LICENSE)
install(FILES "${SEEDLESS_THIRD_PARTY_DIR}/libarchive-3.8.9/libarchive-3.8.9/COPYING"
        DESTINATION share/licenses/seedlessds/dependencies RENAME libarchive-COPYING)
install(FILES "${SEEDLESS_THIRD_PARTY_DIR}/unarr-1.1.1/unarr-1.1.1/COPYING"
        DESTINATION share/licenses/seedlessds/dependencies RENAME unarr-COPYING)

if(SEEDLESS_BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME seedlessds-cli-help COMMAND seedlessds --help)
endif()
if(SEEDLESS_BUILD_TESTING)
    add_test(NAME seedlessds-video-tooling
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_video_tooling.py")
    add_test(NAME seedlessds-prepare-core-offline
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_prepare_core_offline.py")
endif()
if(SEEDLESS_BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(seedlessds-video-layout-test tests/video_layout.c src/video/layout.c)
    target_include_directories(seedlessds-video-layout-test PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_compile_options(seedlessds-video-layout-test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME seedlessds-video-layout COMMAND seedlessds-video-layout-test)

    add_executable(seedlessds-video-mailbox-test tests/video_mailbox.c src/video/frame_mailbox.c)
    target_include_directories(seedlessds-video-mailbox-test PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(seedlessds-video-mailbox-test PRIVATE Threads::Threads)
    target_compile_options(seedlessds-video-mailbox-test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME seedlessds-video-mailbox COMMAND seedlessds-video-mailbox-test)

    add_executable(seedlessds-video-lifecycle-test
        tests/video_lifecycle.c
        src/platform/linux/core_video_bridge.c
        src/video/frame_mailbox.c)
    target_include_directories(seedlessds-video-lifecycle-test PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${SEEDLESS_CORE_SOURCE}/include"
        "${SEEDLESS_CORE_SOURCE}/src")
    target_link_libraries(seedlessds-video-lifecycle-test PRIVATE Threads::Threads)
    target_compile_options(seedlessds-video-lifecycle-test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME seedlessds-video-lifecycle COMMAND seedlessds-video-lifecycle-test)

    add_executable(seedlessds-video-config-test
        tests/test_video_config.c src/config.c src/config_json.c src/launch_options.c)
    target_include_directories(seedlessds-video-config-test PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_compile_options(seedlessds-video-config-test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME seedlessds-video-config COMMAND seedlessds-video-config-test)
    add_test(NAME seedlessds-video-readback
        COMMAND seedlessds-video-probe --readback-test)
endif()
