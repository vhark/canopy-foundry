# Build every native dependency with the selected developer environment, not the newest installed MSVC.
if(NOT DEFINED ENV{VCToolsVersion} OR NOT DEFINED ENV{VSINSTALLDIR}
   OR NOT DEFINED ENV{WindowsSDKVersion})
    message(FATAL_ERROR "The MSVC triplet requires an initialized Visual Studio developer environment")
endif()
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PLATFORM_TOOLSET_VERSION "$ENV{VCToolsVersion}")
string(REGEX REPLACE "[/\\\\]+$" "" VCPKG_VISUAL_STUDIO_PATH "$ENV{VSINSTALLDIR}")
string(REGEX REPLACE "[/\\\\]+$" "" VCPKG_CMAKE_SYSTEM_VERSION "$ENV{WindowsSDKVersion}")
set(VCPKG_ENV_PASSTHROUGH VCToolsVersion VSINSTALLDIR WindowsSDKVersion)
