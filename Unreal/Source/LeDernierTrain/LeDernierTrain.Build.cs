using UnrealBuildTool;
public class LeDernierTrain : ModuleRules
{
    public LeDernierTrain(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "ApplicationCore", "RHI", "OnlineSubsystem", "OnlineSubsystemUtils", "GameplayAbilities", "GameplayTags", "GameplayTasks", "CommonInput", "CommonUI" });
        PublicDependencyModuleNames.AddRange(new[]{"AnimGraphRuntime","AIModule","NavigationSystem"});
        DynamicallyLoadedModuleNames.AddRange(new[] { "OnlineSubsystemSteam", "OnlineSubsystemNull" });
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            RuntimeDependencies.Add($"$(ProjectDir)/Binaries/{Target.Platform}/steam_appid.txt", "$(ProjectDir)/steam_appid.txt");
        if (Target.Type == TargetType.Editor)
            PrivateDependencyModuleNames.Add("UnrealEd");
    }
}
