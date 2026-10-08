using UnrealBuildTool;
public class CanopyFoundryEditorTarget : TargetRules
{
    public CanopyFoundryEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.AddRange(new[] { "CanopyFoundry", "CanopyFoundryEditor" });
    }
}
