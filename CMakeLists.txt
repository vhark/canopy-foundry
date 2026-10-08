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

add_library(canopy_core STATIC core/src/units.cpp)
target_include_directories(canopy_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core/include")
target_compile_features(canopy_core PUBLIC cxx_std_20)
canopy_strict_warnings(canopy_core)
canopy_instrument(canopy_core)
# Core mirrors Unreal's default no-exceptions/no-RTTI policy. Catch2 is kept
# outside this target and retains its own exception-enabled implementation.
if(MSVC)
    target_compile_options(canopy_core PRIVATE /GR- /EHs-c-)
    set_property(TARGET canopy_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(canopy_core PRIVATE -fno-rtti -fno-exceptions)
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
    add_executable(canopy_core_tests core/tests/units_test.cpp)
    target_link_libraries(canopy_core_tests PRIVATE canopy_core Catch2::Catch2WithMain)
    if(MSVC)
        set_property(TARGET canopy_core_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    canopy_strict_warnings(canopy_core_tests)
    canopy_instrument(canopy_core_tests)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
    include(Catch)
    catch_discover_tests(canopy_core_tests)
endif()
