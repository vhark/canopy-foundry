using UnrealBuildTool;
using System.Collections.Generic;
public class CanopyFoundryTarget : TargetRules
{
    public CanopyFoundryTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("CanopyFoundry");
    }
}
