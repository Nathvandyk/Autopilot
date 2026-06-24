#include "UI/SClaudePilotPanel.h"
#include "Controller/FClaudePilotController.h"
#include "Services/FListReconciler.h"
#include "Services/FClaudeBridge.h"
#include "Services/FClaudePilotMonitor.h"
#include "Services/FOllamaClient.h"
#include "Config/ClaudePilotConstants.h"
#include "Model/ClaudePilotSelection.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "SAssetDropTarget.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ClaudePilot"

void SClaudePilotPanel::Construct(const FArguments& InArgs,
	TSharedRef<FClaudePilotController> InController,
	TSharedRef<FListReconciler> InReconciler,
	TSharedRef<FClaudeBridge> InBridge,
	TSharedRef<FClaudePilotMonitor> InMonitor,
	TSharedRef<FOllamaClient> InOllama)
{
	Controller = InController;
	Reconciler = InReconciler;
	Bridge = InBridge;
	Monitor = InMonitor;
	Ollama = InOllama;

	TaskListChangedHandle = Controller->OnTaskListChanged.AddSP(this, &SClaudePilotPanel::RefreshList);
	ContextChangedHandle = Monitor->OnContextChanged.AddSP(this, &SClaudePilotPanel::OnEditorContextChanged);

	// Two object-reference lists, rendered with the native property picker (just
	// like a UPROPERTY(EditAnywhere) TArray<AActor*> on a game class). Rooted so GC
	// keeps them alive while the panel exists.
	FPropertyEditorModule& PropMod = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	PromptSelection = NewObject<UClaudePilotSelection>(GetTransientPackage());
	PromptSelection->AddToRoot();
	PromptSelectionView = PropMod.CreateDetailView(DetailsArgs);
	PromptSelectionView->SetObject(PromptSelection);

	TaskSelection = NewObject<UClaudePilotSelection>(GetTransientPackage());
	TaskSelection->AddToRoot();
	TaskSelectionView = PropMod.CreateDetailView(DetailsArgs);
	TaskSelectionView->SetObject(TaskSelection);

	ChildSlot
	[
		SNew(SVerticalBox)

		// --- Prompt -> Claude ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 8.f, 8.f, 4.f)
		[
			SNew(STextBlock).Text(LOCTEXT("PromptLabel", "Prompt"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 4.f)
		[
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData>) { return true; })
			.OnAssetsDropped(this, &SClaudePilotPanel::OnAssetsDroppedOnPrompt)
			[
				SAssignNew(PromptBox, SMultiLineEditableTextBox)
				.HintText(LOCTEXT("PromptHint", "Tell Claude what to build...  (drop assets here to reference them)"))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SAssignNew(UseChecklistCheck, SCheckBox)
				.IsChecked(ECheckBoxState::Checked)
				.ToolTipText(LOCTEXT("UseChecklistTip", "On: Claude works through the checklist top-to-bottom, then Ollama tidies it. Off: Claude just does your prompt with the tools."))
				[
					SNew(STextBlock).Text(LOCTEXT("UseChecklist", "Use checklist"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SendClaude", "Send to Claude"))
				.OnClicked(this, &SClaudePilotPanel::OnSendClaudeClicked)
			]
		]
		// --- Editor context (the monitor: where you are + Ollama summary) ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 2.f, 8.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SAssignNew(ContextLine, STextBlock)
				.Text(LOCTEXT("CtxInit", "Working in: Level editor"))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.75f, 0.95f)))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("CtxRefresh", "Refresh"))
				.ToolTipText(LOCTEXT("CtxRefreshTip", "Ask Ollama to re-summarize what you're working on"))
				.OnClicked_Lambda([this]() { RefreshContextSummary(); return FReply::Handled(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 4.f)
		[
			SAssignNew(ContextSummaryText, STextBlock)
			.Text(FText::GetEmpty())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.92f)))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 2.f)
		[
			SAssignNew(ClaudeStatus, STextBlock).Text(FText::GetEmpty())
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f)
		[
			SNew(SBox).MaxDesiredHeight(120.f)
			[
				SAssignNew(ResponseBox, SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.HintText(LOCTEXT("ResponseHint", "Claude's reply appears here"))
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f) [ SNew(SSeparator) ]

		// --- Prompt objects (native actor-ref list; like UPROPERTY TArray<AActor*>) ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 4.f, 8.f, 2.f)
		[
			SNew(STextBlock).Text(LOCTEXT("PromptObjLabel", "Prompt objects (Claude acts on these)"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 6.f)
		[
			SNew(SBox).MaxDesiredHeight(170.f)
			[
				PromptSelectionView.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f) [ SNew(SSeparator) ]

		// --- Add a checklist item (Title + Description) ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 8.f, 8.f, 4.f)
		[
			SNew(STextBlock).Text(LOCTEXT("AddLabel", "Add to checklist"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 4.f)
		[
			SAssignNew(TitleBox, SEditableTextBox)
			.HintText(LOCTEXT("TitleHint", "Title (e.g. \"Build the floor\")"))
			.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type CommitType)
			{
				if (CommitType == ETextCommit::OnEnter) { CommitNewTask(); }
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 4.f)
		[
			SNew(SAssetDropTarget)
			.bSupportsMultiDrop(true)
			.OnAreAssetsAcceptableForDrop_Lambda([](TArrayView<FAssetData>) { return true; })
			.OnAssetsDropped(this, &SClaudePilotPanel::OnAssetsDroppedOnDesc)
			[
				SAssignNew(DescBox, SMultiLineEditableTextBox)
				.HintText(LOCTEXT("DescHint", "Description - what to do for this step...  (drop assets here)"))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 2.f)
		[
			SNew(STextBlock).Text(LOCTEXT("TaskObjLabel", "Objects for this step (optional)"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 6.f)
		[
			SNew(SBox).MaxDesiredHeight(150.f)
			[
				TaskSelectionView.ToSharedRef()
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f).HAlign(HAlign_Right)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddToList", "Add to List"))
			.OnClicked(this, &SClaudePilotPanel::OnAddClicked)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f) [ SNew(SSeparator) ]

		// --- The checklist (single ordered list, status in place) ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 4.f, 8.f, 2.f)
		[
			SNew(STextBlock).Text(LOCTEXT("ChecklistLabel", "Checklist"))
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(8.f, 0.f, 8.f, 4.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(TaskListView, SListView<FClaudeTaskPtr>)
				.ListItemsSource(&VisibleTasks)
				.OnGenerateRow(this, &SClaudePilotPanel::OnGenerateTaskRow)
				.SelectionMode(ESelectionMode::Multi)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f).HAlign(HAlign_Right)
		[
			SNew(SButton)
			.Text(LOCTEXT("Done", "Mark Done"))
			.ToolTipText(LOCTEXT("DoneTip", "Mark the selected item(s) done"))
			.OnClicked(this, &SClaudePilotPanel::OnDoneClicked)
		]

		// --- Done (items you/Claude marked complete move here) ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 4.f, 8.f, 2.f)
		[
			SNew(STextBlock).Text(LOCTEXT("DoneLabel", "Done"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f)
		[
			SNew(SBox).MaxDesiredHeight(120.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SAssignNew(DoneListView, SListView<FClaudeTaskPtr>)
					.ListItemsSource(&DoneTasks)
					.OnGenerateRow(this, &SClaudePilotPanel::OnGenerateTaskRow)
					.SelectionMode(ESelectionMode::None)
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f) [ SNew(SSeparator) ]

		// --- Ollama tidy pass ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 4.f, 8.f, 4.f)
		[
			SNew(STextBlock).Text(LOCTEXT("ReportLabel", "Report / instruction (Ollama)"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 4.f)
		[
			SAssignNew(ReportBox, SMultiLineEditableTextBox)
			.HintText(LOCTEXT("ReportHint", "e.g. \"floor and walls are done\" -> Ollama tidies the list"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f).HAlign(HAlign_Right)
		[
			SNew(SButton)
			.Text(LOCTEXT("SendOllama", "Send to Ollama"))
			.OnClicked(this, &SClaudePilotPanel::OnReconcileClicked)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f) [ SNew(SSeparator) ]

		// --- Activity log ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 4.f, 8.f, 2.f)
		[
			SNew(STextBlock).Text(LOCTEXT("LogLabel", "Activity Log"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f)
		[
			SNew(SBox).MaxDesiredHeight(150.f)
			[
				SAssignNew(ActivityLogBox, SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.HintText(LOCTEXT("LogHint", "Live progress appears here"))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f) [ SNew(SSeparator) ]

		// --- Scene optimization (read-only audit -> suggestions) ---
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 4.f, 8.f, 4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("OptimizeLabel", "Scene Optimization"))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("OptimizeBtn", "Optimize Scene"))
				.ToolTipText(LOCTEXT("OptimizeTip", "Claude inspects the scene, objects and scripts (read-only) and suggests improvements"))
				.OnClicked(this, &SClaudePilotPanel::OnOptimizeClicked)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f, 0.f, 8.f, 8.f)
		[
			SNew(SBox).MaxDesiredHeight(180.f)
			[
				SAssignNew(SuggestionsBox, SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.HintText(LOCTEXT("SuggestHint", "Improvement suggestions appear here after an optimize run"))
			]
		]
	];

	RefreshList();

	// Seed the context line + summary from the monitor's current state.
	OnEditorContextChanged();
}

SClaudePilotPanel::~SClaudePilotPanel()
{
	if (Controller.IsValid())
	{
		Controller->OnTaskListChanged.Remove(TaskListChangedHandle);
	}
	if (Monitor.IsValid())
	{
		Monitor->OnContextChanged.Remove(ContextChangedHandle);
	}
	if (PromptSelection)
	{
		PromptSelection->RemoveFromRoot();
		PromptSelection = nullptr;
	}
	if (TaskSelection)
	{
		TaskSelection->RemoveFromRoot();
		TaskSelection = nullptr;
	}
}

FReply SClaudePilotPanel::OnAddClicked()
{
	CommitNewTask();
	return FReply::Handled();
}

void SClaudePilotPanel::CommitNewTask()
{
	if (!Controller.IsValid() || !TitleBox.IsValid())
	{
		return;
	}

	const FString Title = TitleBox->GetText().ToString();
	if (Title.TrimStartAndEnd().IsEmpty())
	{
		return;
	}

	FString Desc = DescBox.IsValid() ? DescBox->GetText().ToString() : FString();

	// Bake any picked objects into this item's description.
	if (TaskSelection && TaskSelection->Actors.Num() > 0)
	{
		FString Objs;
		for (const TObjectPtr<AActor>& A : TaskSelection->Actors)
		{
			if (!A)
			{
				continue;
			}
			if (!Objs.IsEmpty())
			{
				Objs += TEXT(", ");
			}
			Objs += A->GetActorLabel();
		}
		if (!Objs.IsEmpty())
		{
			if (!Desc.IsEmpty())
			{
				Desc += TEXT(" ");
			}
			Desc += FString::Printf(TEXT("[objects: %s]"), *Objs);
		}
	}

	Controller->AddTask(Title, Desc);

	TitleBox->SetText(FText::GetEmpty());
	if (DescBox.IsValid())
	{
		DescBox->SetText(FText::GetEmpty());
	}
	if (TaskSelection)
	{
		TaskSelection->Actors.Empty();
		if (TaskSelectionView.IsValid())
		{
			TaskSelectionView->ForceRefresh();
		}
	}
}

void SClaudePilotPanel::OnAssetsDroppedOnPrompt(const FDragDropEvent& /*Event*/, TArrayView<FAssetData> Assets)
{
	AppendAssetRefs(PromptBox, Assets);
}

void SClaudePilotPanel::OnAssetsDroppedOnDesc(const FDragDropEvent& /*Event*/, TArrayView<FAssetData> Assets)
{
	AppendAssetRefs(DescBox, Assets);
}

void SClaudePilotPanel::AppendAssetRefs(const TSharedPtr<SMultiLineEditableTextBox>& Box, TArrayView<FAssetData> Assets)
{
	if (!Box.IsValid())
	{
		return;
	}
	FString Text = Box->GetText().ToString();
	for (const FAssetData& Asset : Assets)
	{
		const FString Path = Asset.GetObjectPathString();
		if (Path.IsEmpty())
		{
			continue;
		}
		if (!Text.IsEmpty() && !Text.EndsWith(TEXT("\n")) && !Text.EndsWith(TEXT(" ")))
		{
			Text += TEXT(" ");
		}
		Text += Path;
	}
	Box->SetText(FText::FromString(Text));
}

FReply SClaudePilotPanel::OnDoneClicked()
{
	if (Controller.IsValid() && TaskListView.IsValid())
	{
		int32 Count = 0;
		for (const FClaudeTaskPtr& Task : TaskListView->GetSelectedItems())
		{
			Controller->MarkDone(Task);
			++Count;
		}
		if (Count > 0)
		{
			AppendLog(FString::Printf(TEXT("Marked %d item(s) done."), Count));
		}
	}
	return FReply::Handled();
}

FString SClaudePilotPanel::BuildContextBlock() const
{
	if (!Monitor.IsValid())
	{
		return FString();
	}
	FString Block = TEXT("EDITOR CONTEXT (where the user is working right now):\n");
	Block += FString::Printf(TEXT("Active: %s\n"), *Monitor->GetCurrentContext());
	if (!ContextSummary.IsEmpty())
	{
		Block += FString::Printf(TEXT("Doing: %s\n"), *ContextSummary);
	}
	const FString Recent = Monitor->GetRecentActivity(10);
	if (!Recent.IsEmpty())
	{
		Block += TEXT("Recent activity:\n");
		Block += Recent;
	}
	if (PromptSelection && PromptSelection->Actors.Num() > 0)
	{
		Block += TEXT("User-selected objects to act on: ");
		bool bFirst = true;
		for (const TObjectPtr<AActor>& A : PromptSelection->Actors)
		{
			if (!A)
			{
				continue;
			}
			if (!bFirst)
			{
				Block += TEXT(", ");
			}
			Block += A->GetActorLabel();
			bFirst = false;
		}
		Block += TEXT("\n");
	}
	Block += TEXT("If the request refers to where the user is (e.g. \"this graph\", \"here\"), act in that editor/asset.\n\n");
	return Block;
}

void SClaudePilotPanel::OnEditorContextChanged()
{
	if (ContextLine.IsValid() && Monitor.IsValid())
	{
		ContextLine->SetText(FText::FromString(
			FString::Printf(TEXT("Working in: %s"), *Monitor->GetCurrentContext())));
	}
	RefreshContextSummary();
}

void SClaudePilotPanel::RefreshContextSummary()
{
	if (!Ollama.IsValid() || !Monitor.IsValid() || bSummaryInFlight)
	{
		return;
	}
	bSummaryInFlight = true;

	TWeakPtr<SClaudePilotPanel> WeakSelf = StaticCastSharedRef<SClaudePilotPanel>(AsShared());
	Ollama->Chat(ClaudePilot::ContextSummarySystem, Monitor->GetRecentActivity(20),
		[WeakSelf](bool bOk, const FString& Content, const FString& /*Error*/)
		{
			const TSharedPtr<SClaudePilotPanel> Self = WeakSelf.Pin();
			if (!Self.IsValid()) { return; }
			Self->bSummaryInFlight = false;
			if (!bOk) { return; }

			// Ollama runs with format=json -> {"summary":"..."}.
			FString Summary;
			TSharedPtr<FJsonObject> Obj;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
			if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			{
				Obj->TryGetStringField(TEXT("summary"), Summary);
			}
			if (Summary.IsEmpty()) { Summary = Content; }

			Self->ContextSummary = Summary;
			if (Self->ContextSummaryText.IsValid())
			{
				Self->ContextSummaryText->SetText(FText::FromString(Summary));
			}
		});
}

FString SClaudePilotPanel::ComposeAgentPrompt(const FString& UserGoal) const
{
	FString Out = ClaudePilot::AgentSystemPreamble;
	Out += TEXT("\n\n");
	Out += BuildContextBlock();
	Out += TEXT("CURRENT CHECKLIST:\n");

	if (Controller.IsValid() && Controller->GetTasks().Num() > 0)
	{
		for (const FClaudeTaskPtr& T : Controller->GetTasks())
		{
			if (!T.IsValid())
			{
				continue;
			}
			const TCHAR* S = TEXT("pending");
			switch (T->Status)
			{
			case FClaudeTask::EStatus::Pending: S = TEXT("pending"); break;
			case FClaudeTask::EStatus::Running: S = TEXT("running"); break;
			case FClaudeTask::EStatus::Done:    S = TEXT("done");    break;
			case FClaudeTask::EStatus::Failed:  S = TEXT("failed");  break;
			}
			Out += FString::Printf(TEXT("#%d [%s] %s"), T->Id, S, *T->Title);
			if (!T->Description.IsEmpty())
			{
				Out += FString::Printf(TEXT(" - %s"), *T->Description);
			}
			Out += TEXT("\n");
		}
	}
	else
	{
		Out += TEXT("(the checklist is empty)\n");
	}

	Out += TEXT("\nUSER INSTRUCTION:\n");
	Out += UserGoal.TrimStartAndEnd().IsEmpty()
		? FString(TEXT("Execute the checklist above, top to bottom."))
		: UserGoal;
	return Out;
}

FString SClaudePilotPanel::ComposeDirectPrompt(const FString& UserGoal) const
{
	FString Out = ClaudePilot::AgentDirectPreamble;
	Out += TEXT("\n\n");
	Out += BuildContextBlock();
	Out += TEXT("USER INSTRUCTION:\n");
	Out += UserGoal;
	return Out;
}

void SClaudePilotPanel::RunChecklistTidy()
{
	if (!Reconciler.IsValid())
	{
		return;
	}
	AppendLog(TEXT("Asking Ollama to tidy the checklist..."));
	TWeakPtr<SClaudePilotPanel> WeakSelf = StaticCastSharedRef<SClaudePilotPanel>(AsShared());
	Reconciler->ReconcileWithReport(ClaudePilot::ChecklistTidyReport, [WeakSelf](const FString& Summary)
	{
		if (const TSharedPtr<SClaudePilotPanel> Self = WeakSelf.Pin())
		{
			Self->AppendLog(FString::Printf(TEXT("Ollama: %s"), *Summary));
		}
	});
}

FReply SClaudePilotPanel::OnSendClaudeClicked()
{
	if (!Bridge.IsValid() || !PromptBox.IsValid())
	{
		return FReply::Handled();
	}

	const FString UserPrompt = PromptBox->GetText().ToString();
	const bool bUseChecklist = UseChecklistCheck.IsValid() && UseChecklistCheck->IsChecked();

	// Checklist mode can run with an empty prompt (= "execute the checklist");
	// direct mode needs an instruction to act on.
	if (UserPrompt.TrimStartAndEnd().IsEmpty() && !bUseChecklist)
	{
		return FReply::Handled();
	}

	// MCP is always on. Checklist mode injects the live list + run-it instructions;
	// direct mode just hands Claude the tools and the user's prompt.
	const FString Prompt = bUseChecklist ? ComposeAgentPrompt(UserPrompt) : ComposeDirectPrompt(UserPrompt);

	if (ClaudeStatus.IsValid())
	{
		ClaudeStatus->SetText(LOCTEXT("ClaudeWorking", "Claude is working in the editor..."));
	}
	if (ResponseBox.IsValid())
	{
		ResponseBox->SetText(FText::GetEmpty());
	}
	AppendLog(bUseChecklist ? TEXT("Sent to Claude (running the checklist).") : TEXT("Sent to Claude (direct)."));

	TWeakPtr<SClaudePilotPanel> WeakSelf = StaticCastSharedRef<SClaudePilotPanel>(AsShared());
	Bridge->Run(Prompt, /*bWithUnrealMcp*/ true,
		[WeakSelf](const FString& Line)
		{
			const TSharedPtr<SClaudePilotPanel> Self = WeakSelf.Pin();
			if (!Self.IsValid()) { return; }
			Self->AppendLog(Line);
			if (Self->ClaudeStatus.IsValid())
			{
				Self->ClaudeStatus->SetText(FText::FromString(Line.Left(120)));
			}
		},
		[WeakSelf, bUseChecklist](bool bOk, const FString& Output, const FString& Error)
		{
			const TSharedPtr<SClaudePilotPanel> Self = WeakSelf.Pin();
			if (!Self.IsValid()) { return; }
			if (Self->ResponseBox.IsValid())
			{
				Self->ResponseBox->SetText(FText::FromString(bOk ? Output : Error));
			}
			if (Self->ClaudeStatus.IsValid())
			{
				Self->ClaudeStatus->SetText(bOk
					? LOCTEXT("ClaudeDone", "Done.")
					: LOCTEXT("ClaudeError", "Error (see reply box)."));
			}
			Self->AppendLog(bOk ? TEXT("Claude finished.") : FString::Printf(TEXT("Claude error: %s"), *Error));

			// Checklist mode hands off to Ollama to tidy/reorganize the list.
			if (bOk && bUseChecklist)
			{
				Self->RunChecklistTidy();
			}
		});

	return FReply::Handled();
}

FReply SClaudePilotPanel::OnReconcileClicked()
{
	if (Reconciler.IsValid() && ReportBox.IsValid())
	{
		const FString Report = ReportBox->GetText().ToString();
		if (!Report.TrimStartAndEnd().IsEmpty())
		{
			AppendLog(TEXT("Sent report to Ollama..."));
			TWeakPtr<SClaudePilotPanel> WeakSelf = StaticCastSharedRef<SClaudePilotPanel>(AsShared());
			Reconciler->ReconcileWithReport(Report, [WeakSelf](const FString& Summary)
			{
				if (const TSharedPtr<SClaudePilotPanel> Self = WeakSelf.Pin())
				{
					Self->AppendLog(FString::Printf(TEXT("Ollama: %s"), *Summary));
				}
			});
		}
	}
	return FReply::Handled();
}

FReply SClaudePilotPanel::OnOptimizeClicked()
{
	if (!Bridge.IsValid())
	{
		return FReply::Handled();
	}

	FString Prompt = ClaudePilot::SceneAuditPreamble;
	Prompt += TEXT("\n\nInspect the current scene now and report your suggestions.");

	if (SuggestionsBox.IsValid())
	{
		SuggestionsBox->SetText(LOCTEXT("Optimizing", "Analyzing the scene (read-only)..."));
	}
	AppendLog(TEXT("Optimization run started (read-only audit)."));

	TWeakPtr<SClaudePilotPanel> WeakSelf = StaticCastSharedRef<SClaudePilotPanel>(AsShared());
	Bridge->Run(Prompt, /*bWithUnrealMcp*/ true,
		[WeakSelf](const FString& Line)
		{
			if (const TSharedPtr<SClaudePilotPanel> Self = WeakSelf.Pin())
			{
				Self->AppendLog(Line);
			}
		},
		[WeakSelf](bool bOk, const FString& Output, const FString& Error)
		{
			const TSharedPtr<SClaudePilotPanel> Self = WeakSelf.Pin();
			if (!Self.IsValid()) { return; }
			if (Self->SuggestionsBox.IsValid())
			{
				Self->SuggestionsBox->SetText(FText::FromString(bOk ? Output : Error));
			}
			Self->AppendLog(bOk
				? TEXT("Optimization run finished.")
				: FString::Printf(TEXT("Optimize error: %s"), *Error));
		});

	return FReply::Handled();
}

TSharedRef<ITableRow> SClaudePilotPanel::OnGenerateTaskRow(FClaudeTaskPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FString Badge = TEXT("[ ]");
	FLinearColor BadgeColor(0.7f, 0.7f, 0.7f);
	if (Item.IsValid())
	{
		switch (Item->Status)
		{
		case FClaudeTask::EStatus::Pending: Badge = TEXT("[ ]"); BadgeColor = FLinearColor(0.70f, 0.70f, 0.70f); break;
		case FClaudeTask::EStatus::Running: Badge = TEXT("[~]"); BadgeColor = FLinearColor(0.95f, 0.80f, 0.20f); break;
		case FClaudeTask::EStatus::Done:    Badge = TEXT("[x]"); BadgeColor = FLinearColor(0.30f, 0.80f, 0.35f); break;
		case FClaudeTask::EStatus::Failed:  Badge = TEXT("[!]"); BadgeColor = FLinearColor(0.90f, 0.30f, 0.30f); break;
		}
	}

	const FString TitleStr = Item.IsValid() ? FString::Printf(TEXT("#%d  %s"), Item->Id, *Item->Title) : FString();
	const FString DescStr = Item.IsValid() ? Item->Description : FString();

	return SNew(STableRow<FClaudeTaskPtr>, OwnerTable)
		.Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f).VAlign(VAlign_Top)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Badge))
				.ColorAndOpacity(FSlateColor(BadgeColor))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(TitleStr))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(DescStr))
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)))
					.Visibility(DescStr.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
			]
		];
}

void SClaudePilotPanel::RefreshList()
{
	// Partition: done items move to the Done list, everything else stays active.
	VisibleTasks.Reset();
	DoneTasks.Reset();
	if (Controller.IsValid())
	{
		for (const FClaudeTaskPtr& Task : Controller->GetTasks())
		{
			if (!Task.IsValid())
			{
				continue;
			}
			(Task->Status == FClaudeTask::EStatus::Done ? DoneTasks : VisibleTasks).Add(Task);
		}
	}

	// RebuildList (not RequestListRefresh): the task pointers are reused across
	// status changes, so we must force row regeneration or badges go stale.
	if (TaskListView.IsValid())
	{
		TaskListView->RebuildList();
	}
	if (DoneListView.IsValid())
	{
		DoneListView->RebuildList();
	}
}

void SClaudePilotPanel::AppendLog(const FString& Line)
{
	const FString Stamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
	ActivityLogText += FString::Printf(TEXT("[%s] %s\n"), *Stamp, *Line);

	if (ActivityLogBox.IsValid())
	{
		ActivityLogBox->SetText(FText::FromString(ActivityLogText));
	}
}

#undef LOCTEXT_NAMESPACE
