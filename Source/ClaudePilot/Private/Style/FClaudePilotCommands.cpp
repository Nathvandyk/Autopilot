#include "Style/FClaudePilotCommands.h"
#include "Styling/AppStyle.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "ClaudePilot"

FClaudePilotCommands::FClaudePilotCommands()
	: TCommands<FClaudePilotCommands>(
		TEXT("ClaudePilot"),
		LOCTEXT("ClaudePilotContext", "Claude Pilot"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FClaudePilotCommands::RegisterCommands()
{
	UI_COMMAND(OpenPanel, "Claude Pilot", "Open the Claude Pilot panel",
		EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift | EModifierKey::Alt, EKeys::P));
}

#undef LOCTEXT_NAMESPACE
