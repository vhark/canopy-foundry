using UnrealBuildTool;
public class CanopyRuntime : ModuleRules
{
    public CanopyRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "CanopySimExternal" });
    }
}
