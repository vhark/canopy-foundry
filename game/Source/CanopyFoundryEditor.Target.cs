using UnrealBuildTool;
public class CanopyFoundryEditorTarget : TargetRules
{
    public CanopyFoundryEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new[] { "CanopyFoundry", "CanopyFoundryEditor" });
    }
}
