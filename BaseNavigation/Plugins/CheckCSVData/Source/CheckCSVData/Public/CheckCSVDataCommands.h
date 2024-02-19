// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "CheckCSVDataStyle.h"

class FCheckCSVDataCommands : public TCommands<FCheckCSVDataCommands>
{
public:

	FCheckCSVDataCommands()
		: TCommands<FCheckCSVDataCommands>(TEXT("CheckCSVData"), NSLOCTEXT("Contexts", "CheckCSVData", "CheckCSVData Plugin"), NAME_None, FCheckCSVDataStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};