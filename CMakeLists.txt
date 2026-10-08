cmake_minimum_required(VERSION 3.25)

# An explicitly bootstrapped checkout is required; never consult a system vcpkg.
set(CANOPY_VCPKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.work/tools/vcpkg")
set(CANOPY_VCPKG_COMMIT "2750401336fb7c95f6619657a46a7e798661341c")
if(NOT EXISTS "${CANOPY_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    message(FATAL_ERROR "Pinned vcpkg is missing; run uv run --frozen python scripts/bootstrap_native.py")
endif()
find_program(CANOPY_GIT git REQUIRED)
execute_process(
    COMMAND "${CANOPY_GIT}" -C "${CANOPY_VCPKG_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE canopy_vcpkg_head
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE canopy_vcpkg_result
)
if(NOT canopy_vcpkg_result EQUAL 0 OR NOT canopy_vcpkg_head STREQUAL CANOPY_VCPKG_COMMIT)
    message(FATAL_ERROR "vcpkg checkout does not match the approved baseline ${CANOPY_VCPKG_COMMIT}")
endif()
execute_process(
    COMMAND "${CANOPY_GIT}" -C "${CANOPY_VCPKG_ROOT}" status --porcelain --untracked-files=all
    OUTPUT_VARIABLE canopy_vcpkg_changes
    RESULT_VARIABLE canopy_vcpkg_status_result
)
if(NOT canopy_vcpkg_status_result EQUAL 0 OR NOT canopy_vcpkg_changes STREQUAL "")
    message(FATAL_ERROR "Pinned vcpkg checkout has tracked or untracked changes; refusing modified toolchain")
endif()
execute_process(
    COMMAND "${CANOPY_GIT}" -C "${CANOPY_VCPKG_ROOT}" ls-files --others --ignored --exclude-standard -- triplets
    OUTPUT_VARIABLE canopy_vcpkg_custom_triplets
    RESULT_VARIABLE canopy_vcpkg_triplets_result
)
if(NOT canopy_vcpkg_triplets_result EQUAL 0 OR NOT canopy_vcpkg_custom_triplets STREQUAL "")
    message(FATAL_ERROR "Pinned vcpkg checkout contains unapproved custom triplets")
endif()
if(NOT CMAKE_TOOLCHAIN_FILE STREQUAL "${CANOPY_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    message(FATAL_ERROR "Configure using a native CMake preset; global toolchains are not accepted")
endif()
if(NOT VCPKG_MANIFEST_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
    message(FATAL_ERROR "The native dependencies manifest directory must be dependencies/")
endif()
if(NOT VCPKG_INSTALLED_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/.work/vcpkg_installed")
    message(FATAL_ERROR "vcpkg packages must be installed in the isolated .work/vcpkg_installed directory")
endif()

project(canopy_foundry_core VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(FATAL_ERROR "Standalone native builds require Ninja and the checked-in presets")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CANOPY_ENABLE_SANITIZERS "Instrument core and tests with address and undefined-behavior sanitizers" OFF)
if(CANOPY_ENABLE_SANITIZERS)
    if(WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
        message(FATAL_ERROR "ASan + UBSan preset requires native non-Windows Clang/AppleClang/GCC; unsupported compiler/platform")
    endif()
    include(CheckCXXCompilerFlag)
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_cxx_compiler_flag("-fsanitize=address,undefined" CANOPY_HAS_ADDRESS_UNDEFINED_SANITIZERS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT CANOPY_HAS_ADDRESS_UNDEFINED_SANITIZERS)
        message(FATAL_ERROR "Compiler does not support both address and undefined-behavior sanitizers")
    endif()
endif()

function(canopy_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /fp:strict)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-fast-math -ffp-contract=off)
    endif()
endfunction()

function(canopy_instrument target)
    if(CANOPY_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

set(flatbuffers_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/flatbuffers")
set(zstd_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/zstd")
find_package(flatbuffers 25.12.19 EXACT CONFIG REQUIRED PATHS "${flatbuffers_DIR}" NO_DEFAULT_PATH)
find_package(zstd 1.5.7 EXACT CONFIG REQUIRED PATHS "${zstd_DIR}" NO_DEFAULT_PATH)
set(CANOPY_FLATC_TRIPLET "${VCPKG_HOST_TRIPLET}")
if(CANOPY_FLATC_TRIPLET STREQUAL "")
    string(REGEX REPLACE "-ue58$" "" CANOPY_FLATC_TRIPLET "${VCPKG_TARGET_TRIPLET}")
endif()
set(CANOPY_FLATC "${VCPKG_INSTALLED_DIR}/${CANOPY_FLATC_TRIPLET}/tools/flatbuffers/flatc${CMAKE_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${CANOPY_FLATC}")
    message(FATAL_ERROR "Pinned host flatc 25.12.19 is missing from isolated vcpkg install")
endif()
set(CANOPY_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${CANOPY_GENERATED_DIR}/save_generated.h" "${CANOPY_GENERATED_DIR}/command-log_generated.h"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CANOPY_GENERATED_DIR}"
    COMMAND "${CANOPY_FLATC}" --cpp -I "${CMAKE_CURRENT_SOURCE_DIR}/contracts"
        -o "${CANOPY_GENERATED_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/contracts/save.fbs"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/command-log.fbs"
    DEPENDS contracts/save.fbs contracts/command-log.fbs VERBATIM)
add_custom_target(canopy_save_schemas DEPENDS "${CANOPY_GENERATED_DIR}/save_generated.h" "${CANOPY_GENERATED_DIR}/command-log_generated.h")
add_library(canopy_core STATIC
    core/src/units.cpp
    core/src/clock.cpp
    core/src/world.cpp
    core/src/receipts.cpp
    core/src/world_state.cpp
)
add_library(canopy_persistence STATIC
    core/src/persistence/snapshot.cpp
    core/src/persistence/journal.cpp
    core/src/persistence/digest.cpp
    core/src/persistence/manifest.cpp
    core/src/persistence/atomic_file.cpp
)
target_include_directories(canopy_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core/include")
target_compile_features(canopy_core PUBLIC cxx_std_20)
add_dependencies(canopy_persistence canopy_save_schemas)
target_include_directories(canopy_persistence PRIVATE "${CANOPY_GENERATED_DIR}")
target_include_directories(canopy_persistence PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core/include")
target_link_libraries(canopy_persistence PUBLIC canopy_core PRIVATE flatbuffers::flatbuffers zstd::libzstd_static)
canopy_strict_warnings(canopy_persistence)
canopy_instrument(canopy_persistence)
canopy_strict_warnings(canopy_core)
canopy_instrument(canopy_core)
# Match UE 5.8's native policy: no RTTI, with exceptions enabled on Mac.
# Catch2 stays outside this target and retains its own exception support.
if(MSVC)
    target_compile_options(canopy_core PRIVATE /GR- /EHs-c-)
    set_property(TARGET canopy_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
elseif(APPLE)
    target_compile_options(canopy_core PRIVATE -fno-rtti -fexceptions)
else()
    target_compile_options(canopy_core PRIVATE -fno-rtti -fno-exceptions)
endif()
if(MSVC)
    target_compile_options(canopy_persistence PRIVATE /GR- /EHs-c-)
    set_property(TARGET canopy_persistence PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
elseif(APPLE)
    target_compile_options(canopy_persistence PRIVATE -fno-rtti -fexceptions)
else()
    target_compile_options(canopy_persistence PRIVATE -fno-rtti -fno-exceptions)
endif()

include(CTest)
if(BUILD_TESTING)
    # Resolve only the isolated vcpkg install, never a machine-wide Catch2.
    if(NOT VCPKG_TARGET_TRIPLET)
        message(FATAL_ERROR "vcpkg target triplet was not selected by the pinned toolchain")
    endif()
    set(Catch2_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/catch2")
    if(NOT EXISTS "${Catch2_DIR}/Catch2Config.cmake")
        message(FATAL_ERROR "Pinned Catch2 package is missing from the isolated vcpkg install")
    endif()
    find_package(Catch2 3.16.0 EXACT CONFIG REQUIRED PATHS "${Catch2_DIR}" NO_DEFAULT_PATH)
    add_executable(canopy_core_tests
        core/tests/units_test.cpp
        core/tests/clock_test.cpp
        core/tests/commands_test.cpp
        core/tests/replay_test.cpp
        core/tests/save_test.cpp
        core/tests/save_migration_test.cpp
    )
    target_link_libraries(canopy_core_tests PRIVATE canopy_persistence Catch2::Catch2WithMain)
    target_compile_definitions(canopy_core_tests PRIVATE CANOPY_SAVE_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/saves")
    if(MSVC)
        set_property(TARGET canopy_core_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    canopy_strict_warnings(canopy_core_tests)
    canopy_instrument(canopy_core_tests)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
    include(Catch)
    catch_discover_tests(canopy_core_tests)
    add_executable(canopy_save_crash core/tests/save_crash.cpp)
    target_link_libraries(canopy_save_crash PRIVATE canopy_persistence)
    canopy_strict_warnings(canopy_save_crash)
    canopy_instrument(canopy_save_crash)
    if(MSVC)
        set_property(TARGET canopy_save_crash PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()
