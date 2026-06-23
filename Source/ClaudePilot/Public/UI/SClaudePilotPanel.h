#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/FClaudeTask.h"

class FClaudePilotController;
class FListReconciler;
class FClaudeBridge;
class SMultiLineEditableTextBox;
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
		TSharedRef<FClaudeBridge> InBridge);
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

	/** Build the full agent prompt: operating preamble + the live checklist +
	 *  the user's instruction (or "execute the checklist" if none). MCP mode only. */
	FString ComposeAgentPrompt(const FString& UserGoal) const;

	/** Append one timestamped line to the Activity Log. */
	void AppendLog(const FString& Line);

	/** Builds one two-line checklist row (badge + title + description). */
	TSharedRef<ITableRow> OnGenerateTaskRow(FClaudeTaskPtr Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Pull fresh data from the controller into the list view. */
	void RefreshList();

	TSharedPtr<FClaudePilotController> Controller;
	TSharedPtr<FListReconciler> Reconciler;
	TSharedPtr<FClaudeBridge> Bridge;

	// Prompt / Claude
	TSharedPtr<SMultiLineEditableTextBox> PromptBox;
	TSharedPtr<SCheckBox> UseMcpCheck;
	TSharedPtr<STextBlock> ClaudeStatus;
	TSharedPtr<SMultiLineEditableTextBox> ResponseBox;

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
};
