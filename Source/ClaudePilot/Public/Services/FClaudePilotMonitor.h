#pragma once

#include "CoreMinimal.h"

class IAssetEditorInstance;
class SDockTab;

/** Broadcast (game thread) when the active editor context meaningfully changes. */
DECLARE_MULTICAST_DELEGATE(FOnEditorContextChanged);

/**
 * Editor-context monitor.
 *
 * Watches which editor/window the user is in and what they're working on,
 * writes a fresh-per-session activity log, and exposes a current-context string
 * for Claude. Pure capture - it owns no LLM; the panel feeds the recent log to
 * Ollama for a "what you're doing" summary. Hooks asset-editor-opened +
 * active-tab-changed. Plain shared C++ object owned by the module; unhooks in
 * Stop() / the destructor.
 */
class FClaudePilotMonitor : public TSharedFromThis<FClaudePilotMonitor>
{
public:
	FClaudePilotMonitor();
	~FClaudePilotMonitor();

	/** Wipe + open the session log and subscribe to editor events. Call once
	 *  GEditor exists (the module defers to OnPostEngineInit if needed). */
	void Start();
	void Stop();

	/** One-line current focus, e.g. "Material Editor (shader graph) - /Game/.../M_Red". */
	const FString& GetCurrentContext() const { return CurrentContext; }

	/** Recent activity (newest last), for summarizing / prompt injection. */
	FString GetRecentActivity(int32 MaxLines = 20) const;

	const FString& GetLogPath() const { return LogPath; }

	/** Fires after a meaningful context change (active editor / window switch). */
	FOnEditorContextChanged OnContextChanged;

private:
	void HandleAssetOpened(UObject* Asset, IAssetEditorInstance* Instance);
	void HandleActiveTabChanged(TSharedPtr<SDockTab> NewlyActivated, TSharedPtr<SDockTab> Previously);
	void HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	void Append(const FString& Event);          // timestamped -> ring + log file
	void SetContext(const FString& NewContext);  // updates + broadcasts on change

	FString LogPath;
	FString CurrentContext;
	TArray<FString> RecentLines;

	/** Asset display-name -> context string, so focusing an already-open editor
	 *  tab can restore its context without re-opening it. */
	TMap<FString, FString> OpenEditors;

	FDelegateHandle AssetOpenedHandle;
	FDelegateHandle ActiveTabHandle;
	FDelegateHandle SelectionHandle;
	bool bStarted = false;
};
