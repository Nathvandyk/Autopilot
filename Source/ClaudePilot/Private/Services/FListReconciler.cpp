#include "Services/FListReconciler.h"
#include "Services/FOllamaClient.h"
#include "Controller/FClaudePilotController.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogClaudePilot, Log, All);

static const TCHAR* StatusStr(FClaudeTask::EStatus S)
{
	switch (S)
	{
	case FClaudeTask::EStatus::Pending: return TEXT("pending");
	case FClaudeTask::EStatus::Running: return TEXT("running");
	case FClaudeTask::EStatus::Done:    return TEXT("done");
	case FClaudeTask::EStatus::Failed:  return TEXT("failed");
	}
	return TEXT("pending");
}

FListReconciler::FListReconciler(TSharedRef<FClaudePilotController> InController, TSharedRef<FOllamaClient> InOllama)
	: Controller(InController)
	, Ollama(InOllama)
{
}

void FListReconciler::ReconcileWithReport(const FString& Report, TFunction<void(const FString&)> OnDone)
{
	if (!Ollama.IsValid())
	{
		if (OnDone) { OnDone(TEXT("No Ollama client.")); }
		return;
	}

	const FString User = BuildListSnapshot() + TEXT("\n\nREPORT / INSTRUCTION:\n") + Report;

	TWeakPtr<FListReconciler> WeakSelf = AsShared();
	Ollama->Chat(BuildSystemPrompt(), User,
		[WeakSelf, OnDone](bool bOk, const FString& Content, const FString& Error)
		{
			const TSharedPtr<FListReconciler> Self = WeakSelf.Pin();
			if (!Self.IsValid())
			{
				return;
			}

			const FString Summary = bOk
				? Self->ApplyOps(Content)
				: FString::Printf(TEXT("Ollama error: %s"), *Error);

			UE_LOG(LogClaudePilot, Log, TEXT("Reconcile: %s"), *Summary);
			if (OnDone)
			{
				OnDone(Summary);
			}
		});
}

FString FListReconciler::BuildSystemPrompt() const
{
	return FString(
		TEXT("You tidy a build checklist. Each item has an id, a status, a title and a description. ")
		TEXT("You receive the current checklist and a report or instruction. ")
		TEXT("Reply with ONLY a JSON object of the form {\"ops\":[ ... ]} describing how to update it. ")
		TEXT("Valid operations (use exactly these shapes):\n")
		TEXT("{\"op\":\"add\",\"title\":\"<title>\",\"description\":\"<optional details>\"}\n")
		TEXT("{\"op\":\"edit\",\"id\":<id>,\"title\":\"<title>\",\"description\":\"<details>\"}\n")
		TEXT("{\"op\":\"done\",\"id\":<id>}\n")
		TEXT("{\"op\":\"reopen\",\"id\":<id>}\n")
		TEXT("{\"op\":\"move\",\"id\":<id>,\"to\":<zero-based index>}\n")
		TEXT("{\"op\":\"remove\",\"id\":<id>}\n")
		TEXT("Only reference ids that appear in the checklist. Mark items done when the report says they are complete. ")
		TEXT("If nothing should change, return {\"ops\":[]}. Output the JSON only - no prose, no code fences."));
}

FString FListReconciler::BuildListSnapshot() const
{
	FString Out = TEXT("CURRENT CHECKLIST:");
	if (Controller.IsValid())
	{
		const TArray<FClaudeTaskPtr>& Tasks = Controller->GetTasks();
		if (Tasks.Num() == 0)
		{
			Out += TEXT("\n(empty)");
		}
		for (const FClaudeTaskPtr& T : Tasks)
		{
			if (!T.IsValid())
			{
				continue;
			}
			Out += FString::Printf(TEXT("\n- id %d [%s] %s"), T->Id, StatusStr(T->Status), *T->Title);
			if (!T->Description.IsEmpty())
			{
				Out += FString::Printf(TEXT(" - %s"), *T->Description);
			}
		}
	}
	return Out;
}

FString FListReconciler::ApplyOps(const FString& JsonContent) const
{
	if (!Controller.IsValid())
	{
		return TEXT("No controller.");
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return FString::Printf(TEXT("Could not parse model output as JSON: %s"), *JsonContent.Left(200));
	}

	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	if (!Root->TryGetArrayField(TEXT("ops"), Ops))
	{
		return TEXT("Model output had no \"ops\" array.");
	}

	int32 Applied = 0;
	int32 Dropped = 0;
	FString Log;

	for (const TSharedPtr<FJsonValue>& V : *Ops)
	{
		const TSharedPtr<FJsonObject>* OpPtr = nullptr;
		if (!V.IsValid() || !V->TryGetObject(OpPtr) || !OpPtr)
		{
			++Dropped;
			continue;
		}
		const TSharedPtr<FJsonObject>& Op = *OpPtr;

		FString Kind;
		Op->TryGetStringField(TEXT("op"), Kind);
		Kind = Kind.ToLower();

		bool bOk = false;
		FString Desc;

		if (Kind == TEXT("add"))
		{
			FString Title, Description;
			Op->TryGetStringField(TEXT("title"), Title);
			Op->TryGetStringField(TEXT("description"), Description);
			bOk = Controller->AddTask(Title, Description) != 0;
			Desc = FString::Printf(TEXT("add '%s'"), *Title);
		}
		else if (Kind == TEXT("edit"))
		{
			int32 Id = 0; FString Title, Description;
			Op->TryGetNumberField(TEXT("id"), Id);
			Op->TryGetStringField(TEXT("title"), Title);
			Op->TryGetStringField(TEXT("description"), Description);
			bOk = Controller->EditTask(Id, Title, Description);
			Desc = FString::Printf(TEXT("edit #%d"), Id);
		}
		else if (Kind == TEXT("done"))
		{
			int32 Id = 0;
			Op->TryGetNumberField(TEXT("id"), Id);
			bOk = Controller->SetStatus(Id, FClaudeTask::EStatus::Done);
			Desc = FString::Printf(TEXT("done #%d"), Id);
		}
		else if (Kind == TEXT("reopen"))
		{
			int32 Id = 0;
			Op->TryGetNumberField(TEXT("id"), Id);
			bOk = Controller->SetStatus(Id, FClaudeTask::EStatus::Pending);
			Desc = FString::Printf(TEXT("reopen #%d"), Id);
		}
		else if (Kind == TEXT("move"))
		{
			int32 Id = 0; int32 To = 0;
			Op->TryGetNumberField(TEXT("id"), Id);
			Op->TryGetNumberField(TEXT("to"), To);
			bOk = Controller->MoveTask(Id, To);
			Desc = FString::Printf(TEXT("move #%d -> %d"), Id, To);
		}
		else if (Kind == TEXT("remove"))
		{
			int32 Id = 0;
			Op->TryGetNumberField(TEXT("id"), Id);
			bOk = Controller->RemoveTask(Id);
			Desc = FString::Printf(TEXT("remove #%d"), Id);
		}
		else
		{
			Desc = FString::Printf(TEXT("unknown op '%s'"), *Kind);
		}

		if (bOk)
		{
			++Applied;
			Log += FString::Printf(TEXT("\n  + %s"), *Desc);
		}
		else
		{
			++Dropped;
			Log += FString::Printf(TEXT("\n  - dropped: %s"), *Desc);
		}
	}

	return FString::Printf(TEXT("Applied %d, dropped %d.%s"), Applied, Dropped, *Log);
}
