#pragma once

#include "CoreMinimal.h"

class FClaudePilotController;
class FOllamaClient;

/**
 * Domain layer: turns a free-text report/instruction into validated list
 * operations.
 *
 * Flow: snapshot the current checklist -> ask Ollama for a JSON patch
 * ({"ops":[...]}) -> run every proposed op through the controller's
 * deterministic gate -> only legal ops are applied. Ollama proposes; the
 * controller disposes. A hallucinated id or bad index is simply dropped.
 */
class FListReconciler : public TSharedFromThis<FListReconciler>
{
public:
	FListReconciler(TSharedRef<FClaudePilotController> InController, TSharedRef<FOllamaClient> InOllama);

	/** Async. Sends the checklist + report to Ollama and applies the result.
	 *  OnDone(summary) runs on the game thread with a human-readable log. */
	void ReconcileWithReport(const FString& Report, TFunction<void(const FString&)> OnDone = nullptr);

private:
	FString BuildSystemPrompt() const;
	FString BuildListSnapshot() const;

	/** Parse the model's JSON patch and apply each op through the gate. */
	FString ApplyOps(const FString& JsonContent) const;

	TSharedPtr<FClaudePilotController> Controller;
	TSharedPtr<FOllamaClient> Ollama;
};
