// Copyright Epic Games, Inc. All Rights Reserved.

#include "CheckCSVData.h"
#include "CheckCSVDataStyle.h"
#include "CheckCSVDataCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"

static const FName CheckCSVDataTabName("CheckCSVData");

#define LOCTEXT_NAMESPACE "FCheckCSVDataModule"

void FCheckCSVDataModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FCheckCSVDataStyle::Initialize();
	FCheckCSVDataStyle::ReloadTextures();

	FCheckCSVDataCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FCheckCSVDataCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FCheckCSVDataModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCheckCSVDataModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CheckCSVDataTabName, FOnSpawnTab::CreateRaw(this, &FCheckCSVDataModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FCheckCSVDataTabTitle", "CheckCSVData"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FCheckCSVDataModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FCheckCSVDataStyle::Shutdown();

	FCheckCSVDataCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(CheckCSVDataTabName);
}

TSharedRef<SDockTab> FCheckCSVDataModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FCheckCSVDataModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("CheckCSVData.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(WidgetText)
			]
		];
}

void FCheckCSVDataModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(CheckCSVDataTabName);
}

void FCheckCSVDataModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FCheckCSVDataCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				//FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FCheckCSVDataCommands::Get().OpenPluginWindow));
				//Entry.SetCommandList(PluginCommands);
				Section.AddMenuEntryWithCommandList(FCheckCSVDataCommands::Get().OpenPluginWindow, PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCheckCSVDataModule, CheckCSVData)