#pragma once

#include "CoreMinimal.h"
#include "Model/FClaudeTask.h"

/** Broadcast whenever the task list changes so the UI can refresh. */
DECLARE_MULTICAST_DELEGATE(FOnTaskListChanged);

/**
 * Orchestration layer: owns the checklist and is the ONLY thing allowed to
 * mutate it. Knows nothing about Slate or Ollama.
 *
 * Every mutator is a deterministic gate: it validates the operation against the
 * real list (id exists? title non-empty? index in range?), applies it only if
 * legal, and reports success. Ollama (and later Claude's checklist tool) propose
 * operations; these methods decide what actually happens.
 */
class FClaudePilotController
{
public:
	// --- Validated mutation API (the deterministic gate) ---

	/** Append an item. Returns its new id, or 0 if the title was empty. */
	int32 AddTask(const FString& Title, const FString& Description);

	/** Replace an item's title/description. False if id unknown or title empty. */
	bool EditTask(int32 Id, const FString& Title, const FString& Description);

	/** Set an item's lifecycle status. False if the id is unknown. */
	bool SetStatus(int32 Id, FClaudeTask::EStatus NewStatus);

	/** Remove an item. False if the id is unknown. */
	bool RemoveTask(int32 Id);

	/** Move an item to a new position. NewIndex clamped into range. False if id unknown. */
	bool MoveTask(int32 Id, int32 NewIndex);

	/** Remove every item. */
	void ClearTasks();

	// --- UI convenience ---

	/** Mark an item done from its shared pointer (the panel holds these). */
	void MarkDone(const FClaudeTaskPtr& Task);

	// --- Reads ---

	FClaudeTaskPtr FindById(int32 Id) const;
	const TArray<FClaudeTaskPtr>& GetTasks() const { return Tasks; }

	/** Fired after any change to the list. */
	FOnTaskListChanged OnTaskListChanged;

private:
	int32 IndexOfId(int32 Id) const;

	TArray<FClaudeTaskPtr> Tasks;
	int32 NextId = 1;   // monotonic; ids are never reused
};
