cmake_minimum_required(VERSION 4.0)
project(gakumas_VRify LANGUAGES C CXX ASM_MASM RC)
set(VR_COMPILER_PROCESSES 4 CACHE STRING "Maximum parallel compiler processes")
set(VR_BUILD_MODE "Plugin" CACHE STRING "Plugin or Standalone")
set_property(CACHE VR_BUILD_MODE PROPERTY STRINGS Plugin Standalone)
if(NOT VR_BUILD_MODE STREQUAL "Plugin" AND NOT VR_BUILD_MODE STREQUAL "Standalone")
    message(FATAL_ERROR "VR_BUILD_MODE must be Plugin or Standalone")
endif()
if(NOT MSVC OR MSVC_TOOLSET_VERSION LESS 143)
    message(FATAL_ERROR "gakumas-VRify requires Visual Studio 2022+ (v143+)")
endif()
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(ROOT "${PROJECT_ROOT}/build/source")
if(NOT EXISTS "${ROOT}/src/stdinclude.hpp")
    message(FATAL_ERROR "Run build.ps1 to fetch upstream and prepare the combined source tree")
endif()
file(READ "${ROOT}/src/vr/config/defaults.json" VR_DEFAULTS_JSON)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(WRITE "${CMAKE_BINARY_DIR}/generated/VrConfigDefaults.hpp"
    "#pragma once\ninline constexpr char kVrReleaseDefaults[] = R\"vr_defaults(${VR_DEFAULTS_JSON})vr_defaults\";\n")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
foreach(dep cpprestsdk minizip ZLIB nlohmann_json fmt)
    find_package(${dep} CONFIG REQUIRED)
endforeach()
# Expose the resolved header path to the standalone config integration harness.
file(WRITE "${CMAKE_BINARY_DIR}/nlohmann-json-include.txt" "${nlohmann_json_INCLUDE_DIRS}")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${ROOT}/src/*.cpp" "${ROOT}/src/*.rc")
list(FILTER SOURCES EXCLUDE REGEX "\\.inc\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "/src/host/ConfigMigrationTool\\.cpp$")
if(VR_BUILD_MODE STREQUAL "Plugin")
    set(GAKULOADER_SDK_DIR "${PROJECT_ROOT}/sdk/gakuloader" CACHE PATH "GakuLoader ABI v1 include directory")
    if(NOT EXISTS "${GAKULOADER_SDK_DIR}/GakuPlugin.h")
        message(FATAL_ERROR "GakuLoader SDK missing: ${GAKULOADER_SDK_DIR}/GakuPlugin.h")
    endif()
    if(NOT EXISTS "${GAKULOADER_SDK_DIR}/GakuPaths.h")
        message(FATAL_ERROR "GakuLoader optional paths SDK header missing: ${GAKULOADER_SDK_DIR}/GakuPaths.h")
    endif()
    list(FILTER SOURCES EXCLUDE REGEX "/src/(main\\.cpp|windowsPlatform\\.cpp|dllproxy/.*)$")
    list(FILTER SOURCES EXCLUDE REGEX "/src/host/(VrBootstrap\\.cpp|VrWindowsPlatform\\.cpp|localify/ModLoader\\.cpp)$")
    list(FILTER SOURCES EXCLUDE REGEX "/src/hooks/HookManager\\.cpp$")
    list(FILTER SOURCES EXCLUDE REGEX "/src/resourceUpdate/.*$")
    list(FILTER SOURCES EXCLUDE REGEX "/src/gkmsGUI/(gkmsGUIMain\\.cpp|gkmsGUILoop\\.cpp)$")
    set(VR_TARGET GakuVRify)
    set(VR_OUTPUT_ROOT "${PROJECT_ROOT}/build/bin/plugin/x64")
    set(VR_RECEIPT_ARGUMENTS --mode Plugin --sdk-header "${GAKULOADER_SDK_DIR}/GakuPlugin.h")
    add_library(${VR_TARGET} SHARED ${SOURCES})
    target_include_directories(${VR_TARGET} PRIVATE "${GAKULOADER_SDK_DIR}")
    target_compile_definitions(${VR_TARGET} PRIVATE GAKU_VRIFY_HOSTED=1)
else()
    list(FILTER SOURCES EXCLUDE REGEX "/src/host/(GakuVrifyPlugin\\.cpp|HostedPlatform\\.cpp|HostedHooks\\.cpp)$")
    file(GLOB_RECURSE MINHOOK_SOURCES CONFIGURE_DEPENDS "${ROOT}/deps/minhook/src/*.c")
    add_library(minhook STATIC ${MINHOOK_SOURCES})
    target_include_directories(minhook PUBLIC "${ROOT}/deps/minhook/include")
    set(VR_TARGET version)
    set(VR_OUTPUT_ROOT "${PROJECT_ROOT}/build/bin/x64")
    set(VR_RECEIPT_ARGUMENTS --mode Standalone)
    add_library(${VR_TARGET} SHARED ${SOURCES}
        "${ROOT}/src/dllproxy/version.asm"
        "${ROOT}/src/dllproxy/version.def")
    target_link_libraries(${VR_TARGET} PRIVATE minhook)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(verify_staged_sources
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_ROOT}/scripts/source-pipeline.py" verify
    VERBATIM)

add_dependencies(${VR_TARGET} verify_staged_sources)
add_custom_command(TARGET ${VR_TARGET} POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_ROOT}/scripts/source-pipeline.py" record-binary --binary "$<TARGET_FILE:${VR_TARGET}>" ${VR_RECEIPT_ARGUMENTS}
    VERBATIM)
target_include_directories(${VR_TARGET} PRIVATE
    "${ROOT}/src" "${ROOT}/src/imgui" "${ROOT}/deps/rapidjson/include" "${CMAKE_BINARY_DIR}/generated"
    "${ROOT}/deps/openxr/include"
    # Locked support headers retain MH_STATUS declarations. Plugin mode never
    # compiles or links MinHook; all executable hook operations use host leases.
    "${ROOT}/deps/minhook/include")
target_compile_definitions(${VR_TARGET} PRIVATE GAKUMAS_EXTERNAL_HOST=1 IMGUI_USER_CONFIG="host/ImGuiUserConfig.hpp")
target_compile_options(${VR_TARGET} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:/std:c++latest;/utf-8;/MP${VR_COMPILER_PROCESSES};/Zi>"
    "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/O2;/Os>")
target_link_options(${VR_TARGET} PRIVATE /DEBUG /INCREMENTAL:NO)
target_link_libraries(${VR_TARGET} PRIVATE cpprestsdk::cpprest minizip::minizip
    ZLIB::ZLIB nlohmann_json::nlohmann_json fmt::fmt d3d11 d3dcompiler dxgi advapi32)
set_target_properties(${VR_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${VR_OUTPUT_ROOT}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${VR_OUTPUT_ROOT}/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY "${VR_OUTPUT_ROOT}/$<CONFIG>")

add_executable(vr-config-migration
    "${ROOT}/src/host/ConfigMigrationTool.cpp"
    "${ROOT}/src/vr/config/VrConfigStartup.cpp")
add_dependencies(vr-config-migration verify_staged_sources)
target_include_directories(vr-config-migration PRIVATE "${ROOT}/src" "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(vr-config-migration PRIVATE nlohmann_json::nlohmann_json bcrypt)
target_compile_options(vr-config-migration PRIVATE /std:c++20 /utf-8)
set_target_properties(vr-config-migration PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_ROOT}/build/tools/$<CONFIG>")
add_custom_command(TARGET vr-config-migration POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_ROOT}/scripts/source-pipeline.py" record-binary
        --binary "$<TARGET_FILE:vr-config-migration>" --mode Tool VERBATIM)
