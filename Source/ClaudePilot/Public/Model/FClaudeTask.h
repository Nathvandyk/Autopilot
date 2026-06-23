#pragma once

#include "CoreMinimal.h"

/**
 * One item on the build checklist.
 *
 * The user authors Title + Description (the plan); Claude works the list
 * top-to-bottom, flipping Status as it goes. This is the single data contract
 * passed between the UI, the controller, and the reconciler.
 */
struct FClaudeTask
{
	/** Lifecycle of an item, surfaced as a badge in the panel. */
	enum class EStatus : uint8
	{
		Pending,   // not started
		Running,   // Claude is working it now
		Done,      // completed
		Failed,    // attempted, errored
	};

	/** Stable identity. Assigned by the controller, never reused. The handle
	 *  Claude/Ollama refer to when proposing list operations. */
	int32 Id = 0;

	/** Short name of the step. */
	FString Title;

	/** What to do / extra instructions for this step. */
	FString Description;

	EStatus Status = EStatus::Pending;

	FClaudeTask() = default;
	FClaudeTask(int32 InId, const FString& InTitle, const FString& InDescription)
		: Id(InId)
		, Title(InTitle)
		, Description(InDescription)
	{
	}

	bool IsDone() const { return Status == EStatus::Done; }
};

/** Shared-pointer alias - Slate list views hold their items by shared pointer. */
using FClaudeTaskPtr = TSharedPtr<FClaudeTask>;
