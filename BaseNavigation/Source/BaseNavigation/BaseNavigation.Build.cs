// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BaseNavigation : ModuleRules
{
	public BaseNavigation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[] { 
                "Core", 
                "CoreUObject", 
                "Engine", 
                "InputCore", 
                "HeadMountedDisplay", 
                "NavigationSystem", 
                "AIModule", 
                "Niagara",
                //"UnLua",
                "ApplicationCore"//ScrollCanvasPanel
            }
        );
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Slate",
                "SlateCore",
                "UMG"
            }
        );
    }
}
