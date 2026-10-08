using System;
using UnrealBuildTool;

public class CanopyCoreQualificationTarget : TargetRules
{
    public CanopyCoreQualificationTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Program;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        LinkType = TargetLinkType.Monolithic;
        LaunchModuleName = "CanopyCoreQualification";
        CppStandard = CppStandardVersion.Cpp20;
        bCompileAgainstEngine = false;
        bCompileAgainstCoreUObject = false;
        bCompileAgainstApplicationCore = false;
        bBuildWithEditorOnlyData = false;
        bBuildDeveloperTools = false;
        bCompileICU = false;
        bIsBuildingConsoleApplication = true;
        bForceEnableExceptions = false;
        bUsePCHFiles = false;
        bUseUnityBuild = false;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string compilerVersion = Environment.GetEnvironmentVariable("CANOPY_UBT_MSVC_VERSION")
                ?? throw new BuildException("Qualified MSVC toolset version is required");
            string sdkVersion = Environment.GetEnvironmentVariable("CANOPY_UBT_WINDOWS_SDK")
                ?? throw new BuildException("Qualified Windows SDK version is required");
            WindowsPlatform.Compiler = WindowsCompiler.VisualStudio2026;
            WindowsPlatform.CompilerVersion = compilerVersion;
            WindowsPlatform.WindowsSdkVersion = sdkVersion;
            bUseStaticCRT = false;
        }
    }
}
