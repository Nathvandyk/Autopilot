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
	/** Scene actors the user picked for Claude to act on. */
	UPROPERTY(EditAnywhere, Category = "ClaudePilot")
	TArray<TObjectPtr<AActor>> Actors;
};
