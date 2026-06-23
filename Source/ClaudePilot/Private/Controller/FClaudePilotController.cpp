#include "Controller/FClaudePilotController.h"

int32 FClaudePilotController::AddTask(const FString& Title, const FString& Description)
{
	const FString TrimmedTitle = Title.TrimStartAndEnd();
	if (TrimmedTitle.IsEmpty())
	{
		return 0;
	}

	const int32 NewId = NextId++;
	Tasks.Add(MakeShared<FClaudeTask>(NewId, TrimmedTitle, Description.TrimStartAndEnd()));
	OnTaskListChanged.Broadcast();
	return NewId;
}

bool FClaudePilotController::EditTask(int32 Id, const FString& Title, const FString& Description)
{
	const FString TrimmedTitle = Title.TrimStartAndEnd();
	if (TrimmedTitle.IsEmpty())
	{
		return false;
	}

	const int32 Index = IndexOfId(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Tasks[Index]->Title = TrimmedTitle;
	Tasks[Index]->Description = Description.TrimStartAndEnd();
	OnTaskListChanged.Broadcast();
	return true;
}

bool FClaudePilotController::SetStatus(int32 Id, FClaudeTask::EStatus NewStatus)
{
	const int32 Index = IndexOfId(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Tasks[Index]->Status = NewStatus;
	OnTaskListChanged.Broadcast();
	return true;
}

bool FClaudePilotController::RemoveTask(int32 Id)
{
	const int32 Index = IndexOfId(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	Tasks.RemoveAt(Index);
	OnTaskListChanged.Broadcast();
	return true;
}

bool FClaudePilotController::MoveTask(int32 Id, int32 NewIndex)
{
	const int32 Index = IndexOfId(Id);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	const int32 Clamped = FMath::Clamp(NewIndex, 0, Tasks.Num() - 1);
	if (Clamped == Index)
	{
		return true;
	}

	FClaudeTaskPtr Moving = Tasks[Index];
	Tasks.RemoveAt(Index);
	Tasks.Insert(Moving, Clamped);
	OnTaskListChanged.Broadcast();
	return true;
}

void FClaudePilotController::ClearTasks()
{
	Tasks.Reset();
	OnTaskListChanged.Broadcast();
}

void FClaudePilotController::MarkDone(const FClaudeTaskPtr& Task)
{
	if (Task.IsValid() && Task->Status != FClaudeTask::EStatus::Done)
	{
		Task->Status = FClaudeTask::EStatus::Done;
		OnTaskListChanged.Broadcast();
	}
}

FClaudeTaskPtr FClaudePilotController::FindById(int32 Id) const
{
	const int32 Index = IndexOfId(Id);
	return Index == INDEX_NONE ? FClaudeTaskPtr() : Tasks[Index];
}

int32 FClaudePilotController::IndexOfId(int32 Id) const
{
	return Tasks.IndexOfByPredicate([Id](const FClaudeTaskPtr& Task)
	{
		return Task.IsValid() && Task->Id == Id;
	});
}
