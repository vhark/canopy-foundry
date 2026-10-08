using UnrealBuildTool;
public class CanopyFoundry : ModuleRules
{
    public CanopyFoundry(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CanopyRuntime" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "EnhancedInput", "Slate", "SlateCore", "UMG", "Json" });
    }
}
