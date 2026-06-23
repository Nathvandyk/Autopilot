#include "Tools/ClaudePilotChecklistLibrary.h"
#include "ClaudePilotModule.h"
#include "Controller/FClaudePilotController.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogClaudePilotTools, Log, All);

static const TCHAR* StatusName(FClaudeTask::EStatus S)
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

static FClaudeTask::EStatus ParseStatus(const FString& In)
{
	const FString L = In.ToLower();
	if (L == TEXT("running")) { return FClaudeTask::EStatus::Running; }
	if (L == TEXT("done"))    { return FClaudeTask::EStatus::Done; }
	if (L == TEXT("failed"))  { return FClaudeTask::EStatus::Failed; }
	return FClaudeTask::EStatus::Pending;
}

FString UClaudePilotChecklistLibrary::GetChecklistJson()
{
	if (!IsInGameThread())
	{
		UE_LOG(LogClaudePilotTools, Warning, TEXT("GetChecklistJson called off the game thread - ignored."));
		return TEXT("[]");
	}

	FClaudePilotController* C = FClaudePilotModule::GetController();
	if (!C)
	{
		return TEXT("[]");
	}

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FClaudeTaskPtr& T : C->GetTasks())
	{
		if (!T.IsValid())
		{
			continue;
		}
		const TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("id"), T->Id);
		O->SetStringField(TEXT("title"), T->Title);
		O->SetStringField(TEXT("description"), T->Description);
		O->SetStringField(TEXT("status"), StatusName(T->Status));
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Arr, Writer);
	return Out;
}

bool UClaudePilotChecklistLibrary::SetItemStatus(int32 Id, const FString& Status)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogClaudePilotTools, Warning, TEXT("SetItemStatus called off the game thread - ignored."));
		return false;
	}

	FClaudePilotController* C = FClaudePilotModule::GetController();
	return C ? C->SetStatus(Id, ParseStatus(Status)) : false;
}

int32 UClaudePilotChecklistLibrary::AddItem(const FString& Title, const FString& Description)
{
	if (!IsInGameThread())
	{
		UE_LOG(LogClaudePilotTools, Warning, TEXT("AddItem called off the game thread - ignored."));
		return 0;
	}

	FClaudePilotController* C = FClaudePilotModule::GetController();
	return C ? C->AddTask(Title, Description) : 0;
}
