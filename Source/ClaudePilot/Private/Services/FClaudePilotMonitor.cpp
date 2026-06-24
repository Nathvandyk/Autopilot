#include "Services/FClaudePilotMonitor.h"

#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Actor.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "UObject/Class.h"

static const int32 GMaxRingLines = 60;

static FString FriendlyEditorName(const UObject* Asset)
{
	if (!Asset)
	{
		return TEXT("Editor");
	}
	const FString Cls = Asset->GetClass()->GetName();
	if (Cls == TEXT("Material"))                 return TEXT("Material Editor (shader graph)");
	if (Cls == TEXT("MaterialInstanceConstant")) return TEXT("Material Instance Editor");
	if (Cls == TEXT("ControlRigBlueprint"))      return TEXT("Control Rig Editor");
	if (Cls == TEXT("AnimBlueprint"))            return TEXT("Animation Blueprint Editor");
	if (Cls == TEXT("WidgetBlueprint"))          return TEXT("Widget (UMG) Editor");
	if (Cls.EndsWith(TEXT("Blueprint")))         return TEXT("Blueprint Editor");   // incl. plain "Blueprint"
	if (Cls == TEXT("NiagaraSystem"))            return TEXT("Niagara Editor");
	if (Cls == TEXT("Skeleton"))                 return TEXT("Skeleton Editor");
	if (Cls == TEXT("SkeletalMesh"))             return TEXT("Skeletal Mesh Editor");
	if (Cls == TEXT("StaticMesh"))               return TEXT("Static Mesh Editor");
	if (Cls == TEXT("AnimMontage") || Cls.StartsWith(TEXT("AnimSequence")) || Cls.StartsWith(TEXT("BlendSpace")))
		return TEXT("Animation Editor");
	if (Cls.Contains(TEXT("Texture")))           return TEXT("Texture Editor");
	if (Cls.StartsWith(TEXT("World")))           return TEXT("Level Editor");
	return FString::Printf(TEXT("%s Editor"), *Cls);   // anything else: "<Class> Editor"
}

FClaudePilotMonitor::FClaudePilotMonitor()
	: CurrentContext(TEXT("Level editor"))
{
}

FClaudePilotMonitor::~FClaudePilotMonitor()
{
	Stop();
}

void FClaudePilotMonitor::Start()
{
	if (bStarted)
	{
		return;
	}
	bStarted = true;

	LogPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClaudePilot"), TEXT("session_context.log"));

	// Fresh log every session (overwrite).
	const FString Header = FString::Printf(TEXT("[%s] ClaudePilot session context log\n"),
		*FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H.%M.%S")));
	FFileHelper::SaveStringToFile(Header, *LogPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	RecentLines.Reset();
	OpenEditors.Reset();

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetOpenedHandle = AES->OnAssetOpenedInEditor().AddRaw(this, &FClaudePilotMonitor::HandleAssetOpened);
		}
	}
	ActiveTabHandle = FGlobalTabmanager::Get()->OnActiveTabChanged_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(this, &FClaudePilotMonitor::HandleActiveTabChanged));

	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		SelectionHandle = LE.OnActorSelectionChanged().AddRaw(this, &FClaudePilotMonitor::HandleActorSelectionChanged);
	}

	Append(TEXT("Session started (Level editor)."));
}

void FClaudePilotMonitor::Stop()
{
	if (!bStarted)
	{
		return;
	}
	bStarted = false;

	if (GEditor)
	{
		if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AES->OnAssetOpenedInEditor().Remove(AssetOpenedHandle);
		}
	}
	AssetOpenedHandle.Reset();

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->OnActiveTabChanged_Unsubscribe(ActiveTabHandle);
	}
	ActiveTabHandle.Reset();

	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LE = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LE.OnActorSelectionChanged().Remove(SelectionHandle);
	}
	SelectionHandle.Reset();
}

void FClaudePilotMonitor::HandleAssetOpened(UObject* Asset, IAssetEditorInstance* /*Instance*/)
{
	if (!Asset)
	{
		return;
	}
	const FString Path = Asset->GetPathName();
	// Ignore the transient preview objects UE opens alongside a real asset editor
	// (PreviewMaterial, MaterialEditorPreviewParameters, ...) - they live in
	// /Engine/Transient and would clobber the real context.
	if (Path.StartsWith(TEXT("/Engine/Transient")))
	{
		return;
	}
	const FString Editor = FriendlyEditorName(Asset);
	const FString Ctx = FString::Printf(TEXT("%s - %s"), *Editor, *Path);

	OpenEditors.Add(Asset->GetName(), Ctx);
	Append(FString::Printf(TEXT("Opened %s for %s"), *Editor, *Path));
	SetContext(Ctx);
}

void FClaudePilotMonitor::HandleActiveTabChanged(TSharedPtr<SDockTab> NewlyActivated, TSharedPtr<SDockTab> /*Previously*/)
{
	if (!NewlyActivated.IsValid())
	{
		return;
	}
	FString Label = NewlyActivated->GetTabLabel().ToString();
	Label.RemoveFromEnd(TEXT("*"));   // editors append '*' when the asset is dirty
	Label.TrimStartAndEndInline();

	// Restore context only when switching to a KNOWN open asset editor's tab. We do
	// NOT infer the level from viewport tab ids - asset editors (material, etc.) reuse
	// the LevelEditorViewport tab type for their preview, which caused false "back to
	// level" switches. The level is detected via actor selection instead.
	if (const FString* Ctx = OpenEditors.Find(Label))
	{
		Append(FString::Printf(TEXT("Switched to %s"), *Label));
		SetContext(*Ctx);
	}
}

void FClaudePilotMonitor::HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool /*bForceRefresh*/)
{
	// Selecting actors is a clear "working in the level" signal, and gives us the
	// specific actor names. (Selecting nothing leaves the context as-is.)
	if (NewSelection.Num() == 0)
	{
		return;
	}
	FString Names;
	int32 Count = 0;
	for (UObject* Obj : NewSelection)
	{
		const AActor* Actor = Cast<AActor>(Obj);
		if (!Actor)
		{
			continue;
		}
		if (!Names.IsEmpty())
		{
			Names += TEXT(", ");
		}
		Names += Actor->GetActorLabel();
		if (++Count >= 4)
		{
			Names += TEXT(", ...");
			break;
		}
	}
	if (Names.IsEmpty())
	{
		return;
	}
	Append(FString::Printf(TEXT("Selected in level: %s"), *Names));
	SetContext(FString::Printf(TEXT("Level editor - selected: %s"), *Names));
}

void FClaudePilotMonitor::SetContext(const FString& NewContext)
{
	if (NewContext == CurrentContext)
	{
		return;
	}
	CurrentContext = NewContext;
	OnContextChanged.Broadcast();
}

void FClaudePilotMonitor::Append(const FString& Event)
{
	const FString Line = FString::Printf(TEXT("[%s] %s"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Event);

	RecentLines.Add(Line);
	if (RecentLines.Num() > GMaxRingLines)
	{
		RecentLines.RemoveAt(0, RecentLines.Num() - GMaxRingLines);
	}

	if (!LogPath.IsEmpty())
	{
		FFileHelper::SaveStringToFile(Line + TEXT("\n"), *LogPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
			&IFileManager::Get(), FILEWRITE_Append);
	}
}

FString FClaudePilotMonitor::GetRecentActivity(int32 MaxLines) const
{
	const int32 First = FMath::Max(0, RecentLines.Num() - MaxLines);
	FString Out;
	for (int32 i = First; i < RecentLines.Num(); ++i)
	{
		Out += RecentLines[i];
		Out += TEXT("\n");
	}
	return Out;
}
