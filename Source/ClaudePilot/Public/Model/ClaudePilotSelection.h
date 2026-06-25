#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ClaudePilotSelection.generated.h"

class AActor;

/**
 * A tiny transient object whose only job is to back the panel's object-reference
 * list. Rendered in an IDetailsView, this single UPROPERTY gives the native actor
 * picker (dropdown + use-selected + eyedropper) plus array add/remove for free -
 * exactly like a `UPROPERTY(EditAnywhere) TArray<AActor*>` on a game class.
 */
UCLASS()
class UClaudePilotSelection : public UObject
{
	GENERATED_BODY()

public:
	/** Scene actors the user picked for Claude to act on. SOFT (path) references:
	 *  a hard AActor* from this non-world settings object would be a cross-world
	 *  reference and UE clears it (the picker would snap back to None). Soft refs
	 *  store the actor's path, which the picker keeps. */
	UPROPERTY(EditAnywhere, Category = "ClaudePilot")
	TArray<TSoftObjectPtr<AActor>> Actors;
};
