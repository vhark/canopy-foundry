using System;
using System.IO;
using UnrealBuildTool;

public class CanopyCoreQualification : ModuleRules
{
    public CanopyCoreQualification(ReadOnlyTargetRules Target) : base(Target)
    {
        bUseRTTI = false;
        bEnableExceptions = Target.Platform == UnrealTargetPlatform.Mac;
        string include = Environment.GetEnvironmentVariable("CANOPY_CORE_INCLUDE")
            ?? throw new BuildException("Qualified core include directory is required");
        string library = Environment.GetEnvironmentVariable("CANOPY_CORE_LIBRARY")
            ?? throw new BuildException("Qualified core static library is required");
        if (!Directory.Exists(include) || !File.Exists(library))
        {
            throw new BuildException("Qualified core headers or static library are missing");
        }
        PrivateIncludePaths.Add(include);
        PublicAdditionalLibraries.Add(library);
    }
}
