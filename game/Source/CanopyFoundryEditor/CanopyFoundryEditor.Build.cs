using UnrealBuildTool;
public class CanopyFoundryEditor : ModuleRules
{
    public CanopyFoundryEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UnrealEd", "CanopyFoundry", "InterchangeEngine", "Json", "PlatformCryptoContext" });
    }
}
