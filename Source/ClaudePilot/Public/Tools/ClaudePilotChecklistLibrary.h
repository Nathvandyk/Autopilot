#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ClaudePilotChecklistLibrary.generated.h"

/**
 * Static functions the MCP checklist toolset calls to drive the in-editor
 * checklist as Claude works.
 *
 * Deliberately a plain BlueprintFunctionLibrary with NO dependency on the
 * ModelContextProtocol plugin: the Python ToolsetDefinition wraps these, and
 * they reach the live FClaudePilotController via FClaudePilotModule::GetController().
 * Game-thread only (they mutate Slate-bound state); calls off the game thread
 * are ignored rather than risking a crash.
 */
UCLASS()
class CLAUDEPILOT_API UClaudePilotChecklistLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The current checklist as a JSON array string:
	 *  [{"id":1,"title":"...","description":"...","status":"pending"}, ...]. */
	UFUNCTION(BlueprintCallable, Category = "ClaudePilot")
	static FString GetChecklistJson();

	/** Set an item's status. Status is one of "pending"|"running"|"done"|"failed".
	 *  Returns true if the id existed and the change was applied. */
	UFUNCTION(BlueprintCallable, Category = "ClaudePilot")
	static bool SetItemStatus(int32 Id, const FString& Status);

	/** Append a new item. Returns its new id, or 0 if the title was empty / no controller. */
	UFUNCTION(BlueprintCallable, Category = "ClaudePilot")
	static int32 AddItem(const FString& Title, const FString& Description);
};
