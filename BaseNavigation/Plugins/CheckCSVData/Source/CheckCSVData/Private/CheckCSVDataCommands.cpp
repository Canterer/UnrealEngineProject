// Copyright Epic Games, Inc. All Rights Reserved.

#include "CheckCSVDataCommands.h"

#define LOCTEXT_NAMESPACE "FCheckCSVDataModule"

void FCheckCSVDataCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "CheckCSVData", "Bring up CheckCSVData window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
