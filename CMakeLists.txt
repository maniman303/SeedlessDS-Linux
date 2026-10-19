cmake_minimum_required(VERSION 3.28)
project(SeedlessDSLinux VERSION 0.1.0 LANGUAGES C ASM)

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
include("${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake")

add_library(recon_fn STATIC)
include("${CMAKE_SOURCE_DIR}/cmake/SeedlessCore.cmake")

add_executable(seedlessds-headless
    src/main.c
    src/runner.c
    src/platform/linux/platform.c)
target_include_directories(seedlessds-headless PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${SEEDLESS_CORE_SOURCE}/include")
target_link_libraries(seedlessds-headless PRIVATE recon_fn Threads::Threads ${CMAKE_DL_LIBS})
target_compile_options(seedlessds-headless PRIVATE -Wall -Wextra -Werror)
target_link_options(seedlessds-headless PRIVATE -static-libgcc -Wl,--no-undefined)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(SEEDLESS_PRESET STREQUAL "linux-arm64-cross")
    find_program(SEEDLESS_READELF NAMES aarch64-linux-gnu-readelf REQUIRED)
else()
    find_program(SEEDLESS_READELF NAMES readelf REQUIRED)
endif()
add_custom_command(TARGET seedlessds-headless POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/audit-runtime.py"
        --binary "$<TARGET_FILE:seedlessds-headless>"
        --preset "${SEEDLESS_PRESET}"
        --readelf "${SEEDLESS_READELF}"
        --output "${CMAKE_BINARY_DIR}/runtime-inventory.json"
    VERBATIM)

install(TARGETS seedlessds-headless RUNTIME DESTINATION bin)
install(FILES "${CMAKE_SOURCE_DIR}/external/SeedlessDS-Core/LICENSE"
        DESTINATION share/licenses/seedlessds-core RENAME LICENSE)
install(FILES "${CMAKE_BINARY_DIR}/runtime-inventory.json"
        DESTINATION share/seedlessds RENAME runtime-inventory.json)
install(FILES "${CMAKE_SOURCE_DIR}/licenses/Lua.txt"
        DESTINATION share/licenses/seedlessds/dependencies RENAME Lua.txt)
install(FILES "${CMAKE_SOURCE_DIR}/licenses/LZMA-SDK-9.20.txt"
        DESTINATION share/licenses/seedlessds/dependencies RENAME LZMA-SDK-9.20.txt)
install(FILES "${seedless_zlib_SOURCE_DIR}/LICENSE"
        DESTINATION share/licenses/seedlessds/dependencies RENAME zlib-LICENSE)
install(FILES "${SEEDLESS_THIRD_PARTY_DIR}/libarchive-3.8.9/libarchive-3.8.9/COPYING"
        DESTINATION share/licenses/seedlessds/dependencies RENAME libarchive-COPYING)
install(FILES "${SEEDLESS_THIRD_PARTY_DIR}/unarr-1.1.1/unarr-1.1.1/COPYING"
        DESTINATION share/licenses/seedlessds/dependencies RENAME unarr-COPYING)

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME seedlessds-cli-help COMMAND seedlessds-headless --help)
endif()
