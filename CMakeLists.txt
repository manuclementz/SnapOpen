# This file configures the build of this SKSE plugin:
# - how to compile the C++ plugin (in addition to settings found in CMakePresets.json)
# - where to output the resulting .dll file

cmake_minimum_required(VERSION 3.21)

# [BEGIN set_project_name]
project(SnapOpen VERSION 1.0.0 LANGUAGES CXX)
# [END set_project_name]

# #
# YOU PROBABLY DON'T NEED TO EDIT ANYTHING BELOW HERE
# #

# If you're not using a mod manager, output straight into your Skyrim "Data" folder.
# Set the `SKYRIM_FOLDER` environment variable to the path of your Skyrim Special Edition folder.
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

# If you're using Mod Organizer 2 or Vortex, output into "<mods>/<this project>".
# Set the `SKYRIM_MODS_FOLDER` environment variable to the path of your "mods" folder.
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()

# Otherwise, set OUTPUT_FOLDER manually if you'd like:
# set(OUTPUT_FOLDER "C:/path/to/any/folder")

# CommonLibSSE-NG is vendored as a git submodule (extern/commonlibsse-ng, pinned to a tag)
# rather than pulled from vcpkg: the only vcpkg-hosted copy available is unmaintained and
# doesn't recognize newer Skyrim runtimes. This builds it from source as part of our own
# project instead.
set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE-NG's own test suite" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "Enable trampoline support for function hooking" FORCE)
add_subdirectory(extern/commonlibsse-ng)
include(extern/commonlibsse-ng/cmake/CommonLibSSE.cmake) # add_commonlibsse_plugin() isn't auto-included for source builds

add_commonlibsse_plugin(${PROJECT_NAME} SOURCES plugin.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE PCH.h)

if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    # Only deploys the ini once - re-running configure shouldn't stomp on settings you've
    # already changed in-place.
    if(NOT EXISTS "${DLL_FOLDER}/${PROJECT_NAME}.ini")
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.ini" DESTINATION "${DLL_FOLDER}")
    endif()

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()
