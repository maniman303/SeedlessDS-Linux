include(FetchContent)

set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/downloads/fetchcontent" CACHE PATH "Target-specific dependency source cache")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Do not update pinned dependency sources")

set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build zlib statically" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build zlib statically" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable dependency test targets" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "Do not install dependency targets separately" FORCE)
FetchContent_Declare(seedless_zlib
    URL https://zlib.net/fossils/zlib-1.3.2.tar.gz
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(seedless_zlib)
set(SEEDLESS_ZLIB_INCLUDE_DIR "${seedless_zlib_SOURCE_DIR}")
set(ZLIB_INCLUDE_DIR "${seedless_zlib_SOURCE_DIR}" CACHE PATH "Pinned static zlib headers" FORCE)
set(ZLIB_LIBRARY_RELEASE zlibstatic CACHE STRING "Pinned static zlib target" FORCE)
set(ZLIB_LIBRARY_DEBUG zlibstatic CACHE STRING "Pinned static zlib target" FORCE)
set(ZLIB_LIBRARIES zlibstatic CACHE STRING "Pinned static zlib target" FORCE)
if(NOT TARGET zlibstatic)
    message(FATAL_ERROR "The pinned zlib did not provide the required static target zlibstatic")
endif()
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()
if(NOT TARGET z)
    add_library(z ALIAS zlibstatic)
endif()

if(NOT DEFINED SEEDLESS_CORE_SOURCE OR NOT IS_DIRECTORY "${SEEDLESS_CORE_SOURCE}/third_party")
    message(FATAL_ERROR "SEEDLESS_CORE_SOURCE must point to a prepared SeedlessDS-Core source tree")
endif()
if(NOT EXISTS "${SEEDLESS_CORE_SOURCE}/third_party/lua/lapi.c")
    message(FATAL_ERROR "Pinned Lua source is missing; run scripts/prepare-core.sh for this preset")
endif()

set(SEEDLESS_THIRD_PARTY_DIR "${CMAKE_BINARY_DIR}/downloads/core-third-party" CACHE PATH "Pinned core dependency download cache")
add_subdirectory("${SEEDLESS_CORE_SOURCE}/third_party" "${CMAKE_BINARY_DIR}/core-third-party")

foreach(required_target seedless_lua seedless_libarchive seedless_unarr seedless_lzma zlibstatic)
    if(NOT TARGET "${required_target}")
        message(FATAL_ERROR "Required static dependency target was not created: ${required_target}")
    endif()
endforeach()


execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libm.a
    RESULT_VARIABLE seedless_libm_result OUTPUT_VARIABLE SEEDLESS_STATIC_LIBM
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT seedless_libm_result EQUAL 0 OR NOT EXISTS "${SEEDLESS_STATIC_LIBM}")
    message(FATAL_ERROR "The selected target compiler does not provide a static libm archive")
endif()
target_link_libraries(seedless_lua PUBLIC "${SEEDLESS_STATIC_LIBM}")
