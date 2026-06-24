#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/FClaudeTask.h"

class FClaudePilotController;
class FListReconciler;
class FClaudeBridge;
class FClaudePilotMonitor;
class FOllamaClient;
class FDragDropEvent;
class SMultiLineEditableTextBox;
class IDetailsView;
class UClaudePilotSelection;
class AActor;
struct FAssetData;
class SEditableTextBox;
class SCheckBox;
class STextBlock;
class ITableRow;
class STableViewBase;
template <typename ItemType> class SListView;

/**
 * The dockable panel.
 *
 * Top to bottom: the Prompt box (-> claude -p, optionally with MCP), an
 * add-to-checklist area (Title + Description), the checklist itself (one ordered
 * list, each row showing title + description + a status badge), and below that
 * the Ollama report box and a live Activity Log.
 *
 * Pure view. Every action is forwarded to the controller / services; it never
 * mutates task state directly and refreshes from the controller's delegate.
 */
class SClaudePilotPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClaudePilotPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs,
		TSharedRef<FClaudePilotController> InController,
		TSharedRef<FListReconciler> InReconciler,
		TSharedRef<FClaudeBridge> InBridge,
		TSharedRef<FClaudePilotMonitor> InMonitor,
		TSharedRef<FOllamaClient> InOllama);
	virtual ~SClaudePilotPanel() override;

private:
	// Button handlers - thin shims onto the controller / services.
	FReply OnAddClicked();
	FReply OnDoneClicked();
	FReply OnReconcileClicked();
	FReply OnSendClaudeClicked();
	FReply OnOptimizeClicked();

	/** Reads Title + Description, queues the item, and clears the fields. */
	void CommitNewTask();

	/** Drag-and-drop: append dropped asset reference paths into the prompt / desc. */
	void OnAssetsDroppedOnPrompt(const FDragDropEvent& Event, TArrayView<FAssetData> Assets);
	void OnAssetsDroppedOnDesc(const FDragDropEvent& Event, TArrayView<FAssetData> Assets);
	void AppendAssetRefs(const TSharedPtr<SMultiLineEditableTextBox>& Box, TArrayView<FAssetData> Assets);

	/** Build the full agent prompt: operating preamble + the live checklist +
	 *  the user's instruction (or "execute the checklist" if none). MCP mode only. */
	FString ComposeAgentPrompt(const FString& UserGoal) const;

	/** Direct mode (checklist OFF): the tools intro + the user's prompt, no list. */
	FString ComposeDirectPrompt(const FString& UserGoal) const;

	/** After a checklist run completes: ask Ollama to tidy/reorganize the list. */
	void RunChecklistTidy();

	/** The "EDITOR CONTEXT" block (where the user is + Ollama summary + recent
	 *  activity) prepended to Claude prompts so he acts in the right place. */
	FString BuildContextBlock() const;

	/** Monitor reported a context change: refresh the line + re-summarize. */
	void OnEditorContextChanged();

	/** Ask Ollama to summarize recent editor activity into ContextSummary. */
	void RefreshContextSummary();

	/** Append one timestamped line to the Activity Log. */
	void AppendLog(const FString& Line);

	/** Builds one two-line checklist row (badge + title + description). */
	TSharedRef<ITableRow> OnGenerateTaskRow(FClaudeTaskPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Pull fresh data from the controller into the list view. */
	void RefreshList();

	TSharedPtr<FClaudePilotController> Controller;
	TSharedPtr<FListReconciler> Reconciler;
	TSharedPtr<FClaudeBridge> Bridge;
	TSharedPtr<FClaudePilotMonitor> Monitor;
	TSharedPtr<FOllamaClient> Ollama;

	// Prompt / Claude
	TSharedPtr<SMultiLineEditableTextBox> PromptBox;
	TSharedPtr<SCheckBox> UseChecklistCheck;
	TSharedPtr<STextBlock> ClaudeStatus;
	TSharedPtr<SMultiLineEditableTextBox> ResponseBox;

	// Editor-context monitor display
	TSharedPtr<STextBlock> ContextLine;
	TSharedPtr<STextBlock> ContextSummaryText;
	FString ContextSummary;
	bool bSummaryInFlight = false;

	// Object-reference picker lists (native details views over UPROPERTY arrays).
	// One for the prompt/run, one baked into a new checklist item. Both rooted.
	UClaudePilotSelection* PromptSelection = nullptr;
	TSharedPtr<IDetailsView> PromptSelectionView;
	UClaudePilotSelection* TaskSelection = nullptr;
	TSharedPtr<IDetailsView> TaskSelectionView;

	// New checklist item
	TSharedPtr<SEditableTextBox> TitleBox;
	TSharedPtr<SMultiLineEditableTextBox> DescBox;

	// The checklist (active items) + a separate Done list completed items move to.
	TSharedPtr<SListView<FClaudeTaskPtr>> TaskListView;
	TArray<FClaudeTaskPtr> VisibleTasks;
	TSharedPtr<SListView<FClaudeTaskPtr>> DoneListView;
	TArray<FClaudeTaskPtr> DoneTasks;

	// Ollama + log
	TSharedPtr<SMultiLineEditableTextBox> ReportBox;
	TSharedPtr<SMultiLineEditableTextBox> ActivityLogBox;
	FString ActivityLogText;

	// Scene optimization (read-only audit -> suggestions)
	TSharedPtr<SMultiLineEditableTextBox> SuggestionsBox;

	FDelegateHandle TaskListChangedHandle;
	FDelegateHandle ContextChangedHandle;
};
