// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BaseNavigation : ModuleRules
{
	public BaseNavigation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "NavigationSystem", "AIModule", "Niagara" });
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Slate",
                "SlateCore",
                "UMG"
            }
        );
    }
}
