#pragma once

#include "CoreMinimal.h"

class FMonitoredProcess;

/**
 * Services / IO layer: runs a prompt through the local Claude Code CLI on the
 * user's Pro/Max *subscription* (not the paid API), streaming progress live.
 *
 * Mechanism (mirrors the proven Blender add-on path): shell out to `claude -p`
 * with ANTHROPIC_API_KEY cleared for the child so it uses the subscription. We
 * ask for `--output-format stream-json --verbose` and read the process output
 * as it arrives, so callers see each step in real time. The prompt is fed via a
 * temp file redirected into stdin to dodge command-line length/quoting limits.
 */
class FClaudeBridge : public TSharedFromThis<FClaudeBridge>
{
public:
	FClaudeBridge(const FString& InClaudePath, const FString& InMcpUrl);
	~FClaudeBridge();

	/** Run one prompt. OnProgress(line) fires on the game thread for each step
	 *  Claude reports; OnDone(bOk, FinalText, Error) fires on the game thread at
	 *  the end. When bWithUnrealMcp is true, Claude is handed Epic's in-engine
	 *  MCP server so it can act in the editor. */
	void Run(const FString& Prompt, bool bWithUnrealMcp,
		TFunction<void(const FString& /*ProgressLine*/)> OnProgress,
		TFunction<void(bool /*bOk*/, const FString& /*FinalText*/, const FString& /*Error*/)> OnDone);

	bool IsBusy() const { return ActiveProc.IsValid(); }

	void SetClaudePath(const FString& InPath) { ClaudePath = InPath; }
	void SetMcpUrl(const FString& InUrl) { McpUrl = InUrl; CachedMcpConfigPath.Empty(); }

private:
	/** Write the MCP config to a stable path once, then reuse it. */
	FString EnsureMcpConfig();

	FString ClaudePath;
	FString McpUrl;
	FString CachedMcpConfigPath;
	TSharedPtr<FMonitoredProcess> ActiveProc;   // the in-flight run, if any
};
