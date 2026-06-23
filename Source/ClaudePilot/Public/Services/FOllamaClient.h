#pragma once

#include "CoreMinimal.h"

/**
 * Services / IO layer: a thin async HTTP client for a local Ollama server.
 *
 * It knows nothing about tasks, checklists or Claude - it just sends a
 * system+user message and hands back the model's reply. `format=json` is
 * always requested so callers can rely on parseable output. The completion
 * callback fires on the game thread, so callers may touch UI/controller state
 * directly from it.
 */
class FOllamaClient
{
public:
	FOllamaClient(const FString& InBaseUrl, const FString& InModel);

	/** Send one chat turn. OnDone(bOk, Content, Error) runs on the game thread. */
	void Chat(const FString& System, const FString& User,
		TFunction<void(bool /*bOk*/, const FString& /*Content*/, const FString& /*Error*/)> OnDone);

	void SetEndpoint(const FString& InBaseUrl, const FString& InModel);
	const FString& GetModel() const { return Model; }

private:
	FString BaseUrl;
	FString Model;
};
