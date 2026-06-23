#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * The editor commands exposed by the plugin. For now there is just one:
 * the command that opens the Claude Pilot panel (used by the Window menu
 * entry). Kept in its own file so menu/command wiring stays out of both the
 * UI and the orchestrator.
 */
class FClaudePilotCommands : public TCommands<FClaudePilotCommands>
{
public:
	FClaudePilotCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> OpenPanel;
};
