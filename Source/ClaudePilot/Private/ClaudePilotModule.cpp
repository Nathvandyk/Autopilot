#include "ClaudePilotModule.h"
#include "Style/FClaudePilotCommands.h"
#include "Controller/FClaudePilotController.h"
#include "Services/FOllamaClient.h"
#include "Services/FListReconciler.h"
#include "Services/FClaudeBridge.h"
#include "Config/ClaudePilotConstants.h"
#include "UI/SClaudePilotPanel.h"

#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ClaudePilot"

static const FName ClaudePilotTabName("ClaudePilot");

// Raw pointer to the live controller so in-engine tools (the MCP checklist
// toolset) can reach it without a UObject. Set in StartupModule, cleared before
// the controller is destroyed in ShutdownModule.
static FClaudePilotController* GChecklistController = nullptr;

FClaudePilotController* FClaudePilotModule::GetController()
{
	return GChecklistController;
}

void FClaudePilotModule::StartupModule()
{
	// 1. Lowest layers first: controller (owns the list), Ollama client (IO),
	//    then the reconciler that wires them together.
	Controller = MakeShared<FClaudePilotController>();
	GChecklistController = Controller.Get();
	Ollama = MakeShared<FOllamaClient>(ClaudePilot::DefaultOllamaUrl, ClaudePilot::DefaultOllamaModel);
	Reconciler = MakeShared<FListReconciler>(Controller.ToSharedRef(), Ollama.ToSharedRef());
	Bridge = MakeShared<FClaudeBridge>(ClaudePilot::DefaultClaudePath, ClaudePilot::DefaultMcpUrl);

	// 2. Commands + the action that opens the panel.
	FClaudePilotCommands::Register();
	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(
		FClaudePilotCommands::Get().OpenPanel,
		FExecuteAction::CreateRaw(this, &FClaudePilotModule::OpenPanelTab),
		FCanExecuteAction());

	// 3. Menu entry - deferred until ToolMenus is ready.
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FClaudePilotModule::RegisterMenus));

	// 4. Dockable tab spawner. Hidden from the default tab list - we surface it
	//    through our own Window-menu entry instead.
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			ClaudePilotTabName,
			FOnSpawnTab::CreateRaw(this, &FClaudePilotModule::OnSpawnPanelTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Claude Pilot"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FClaudePilotModule::ShutdownModule()
{
	// Tear down in reverse order of StartupModule.
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ClaudePilotTabName);

	FClaudePilotCommands::Unregister();

	CommandList.Reset();
	Bridge.Reset();
	Reconciler.Reset();
	Ollama.Reset();
	GChecklistController = nullptr;
	Controller.Reset();
}

TSharedRef<SDockTab> FClaudePilotModule::OnSpawnPanelTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SClaudePilotPanel, Controller.ToSharedRef(), Reconciler.ToSharedRef(), Bridge.ToSharedRef())
		];
}

void FClaudePilotModule::OpenPanelTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ClaudePilotTabName);
}

void FClaudePilotModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& Section = WindowMenu->FindOrAddSection("WindowLayout");
	Section.AddMenuEntryWithCommandList(FClaudePilotCommands::Get().OpenPanel, CommandList);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FClaudePilotModule, ClaudePilot)
