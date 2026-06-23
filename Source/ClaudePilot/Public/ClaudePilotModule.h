#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FClaudePilotController;
class FOllamaClient;
class FListReconciler;
class FClaudeBridge;
class FUICommandList;
class SDockTab;
class FSpawnTabArgs;

/**
 * Thin orchestrator (the entry point).
 *
 * Holds NO task or UI logic. Its only job is lifecycle: construct the
 * controller, register the command + Window-menu entry + dockable tab, and
 * tear them all down in reverse on shutdown.
 */
class FClaudePilotModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** The live checklist controller, for in-engine tools (the MCP checklist
	 *  toolset's BlueprintFunctionLibrary). Null when the module isn't loaded. */
	static FClaudePilotController* GetController();

private:
	/** Builds the dockable tab that hosts the panel. */
	TSharedRef<SDockTab> OnSpawnPanelTab(const FSpawnTabArgs& Args);

	/** Command action - opens/focuses the tab. */
	void OpenPanelTab();

	/** Adds the entry under the editor's Window menu. */
	void RegisterMenus();

	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<FClaudePilotController> Controller;
	TSharedPtr<FOllamaClient> Ollama;
	TSharedPtr<FListReconciler> Reconciler;
	TSharedPtr<FClaudeBridge> Bridge;
};
