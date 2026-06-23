#include "Services/FClaudeBridge.h"

#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/MonitoredProcess.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

/** Per-run state shared between the streaming callbacks. */
struct FClaudeRun
{
	FString LineBuffer;     // accumulates partial output until a full line arrives
	FString FinalOutput;    // the model's final "result" text
	TFunction<void(const FString&)> OnProgress;
	TFunction<void(bool, const FString&, const FString&)> OnDone;
};

static void EmitProgress(FClaudeRun& R, const FString& Line)
{
	if (R.OnProgress && !Line.IsEmpty())
	{
		R.OnProgress(Line);
	}
}

/** Turn one stream-json line into a readable progress line (and capture the result). */
static void ParseStreamLine(FClaudeRun& R, const FString& RawLine)
{
	const FString Line = RawLine.TrimStartAndEnd();
	if (Line.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		// Not JSON - likely a stderr line merged in via 2>&1. Show it raw.
		EmitProgress(R, Line.Left(300));
		return;
	}

	FString Type;
	Obj->TryGetStringField(TEXT("type"), Type);

	if (Type == TEXT("assistant"))
	{
		const TSharedPtr<FJsonObject>* Msg = nullptr;
		if (Obj->TryGetObjectField(TEXT("message"), Msg) && Msg)
		{
			const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
			if ((*Msg)->TryGetArrayField(TEXT("content"), Content))
			{
				for (const TSharedPtr<FJsonValue>& V : *Content)
				{
					const TSharedPtr<FJsonObject>* B = nullptr;
					if (!V.IsValid() || !V->TryGetObject(B) || !B)
					{
						continue;
					}
					FString BType;
					(*B)->TryGetStringField(TEXT("type"), BType);
					if (BType == TEXT("text"))
					{
						FString Text;
						(*B)->TryGetStringField(TEXT("text"), Text);
						Text = Text.TrimStartAndEnd();
						if (!Text.IsEmpty())
						{
							EmitProgress(R, FString::Printf(TEXT("-> %s"), *Text));
						}
					}
					else if (BType == TEXT("tool_use"))
					{
						FString Name;
						(*B)->TryGetStringField(TEXT("name"), Name);
						EmitProgress(R, FString::Printf(TEXT("[tool] %s"), *Name));
					}
				}
			}
		}
	}
	else if (Type == TEXT("user"))
	{
		EmitProgress(R, TEXT("[tool result]"));
	}
	else if (Type == TEXT("result"))
	{
		FString Res;
		Obj->TryGetStringField(TEXT("result"), Res);
		if (!Res.IsEmpty())
		{
			R.FinalOutput = Res;
		}
		EmitProgress(R, TEXT("[done]"));
	}
	else if (Type == TEXT("system"))
	{
		FString Sub;
		Obj->TryGetStringField(TEXT("subtype"), Sub);
		if (Sub == TEXT("init"))
		{
			EmitProgress(R, TEXT("session started"));
		}
	}
}

/** Pull every complete line out of the buffer and parse it. */
static void DrainBuffer(FClaudeRun& R)
{
	int32 NL = INDEX_NONE;
	while (R.LineBuffer.FindChar(TEXT('\n'), NL))
	{
		FString OneLine = R.LineBuffer.Left(NL);
		R.LineBuffer.RightChopInline(NL + 1);
		OneLine.RemoveFromEnd(TEXT("\r"));
		ParseStreamLine(R, OneLine);
	}
}

static void FlushBuffer(FClaudeRun& R)
{
	if (!R.LineBuffer.IsEmpty())
	{
		FString OneLine = R.LineBuffer;
		R.LineBuffer.Empty();
		OneLine.RemoveFromEnd(TEXT("\r"));
		ParseStreamLine(R, OneLine);
	}
}

FClaudeBridge::FClaudeBridge(const FString& InClaudePath, const FString& InMcpUrl)
	: ClaudePath(InClaudePath)
	, McpUrl(InMcpUrl)
{
}

FClaudeBridge::~FClaudeBridge()
{
	if (ActiveProc.IsValid())
	{
		ActiveProc->Cancel(true);
		ActiveProc.Reset();
	}
}

FString FClaudeBridge::EnsureMcpConfig()
{
	if (!CachedMcpConfigPath.IsEmpty() && FPaths::FileExists(CachedMcpConfigPath))
	{
		return CachedMcpConfigPath;
	}

	const FString CfgFile = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClaudePilot_mcp.json"));
	// Server name matches Epic's GenerateClientConfig output ("unreal-mcp") so the
	// tool namespace (mcp__unreal-mcp__*) is consistent across plugin + Desktop.
	const FString Cfg = FString::Printf(
		TEXT("{\"mcpServers\":{\"unreal-mcp\":{\"type\":\"http\",\"url\":\"%s\"}}}"), *McpUrl);

	if (!FFileHelper::SaveStringToFile(Cfg, *CfgFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return FString();
	}

	CachedMcpConfigPath = CfgFile;
	return CfgFile;
}

void FClaudeBridge::Run(const FString& Prompt, bool bWithUnrealMcp,
	TFunction<void(const FString&)> OnProgress,
	TFunction<void(bool, const FString&, const FString&)> OnDone)
{
	if (ActiveProc.IsValid())
	{
		OnDone(false, FString(), TEXT("A Claude run is already in progress."));
		return;
	}

	const FString TempDir = FPlatformProcess::UserTempDir();

	// Prompt -> temp file (avoids command-line length/quoting limits).
	const FString PromptFile = FPaths::Combine(TempDir, TEXT("claudepilot_prompt.txt"));
	if (!FFileHelper::SaveStringToFile(Prompt, *PromptFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OnDone(false, FString(), TEXT("Could not write the prompt temp file."));
		return;
	}

	// Optional: hand Claude Epic's in-engine MCP server so it can act in the editor.
	FString ToolFlags;
	if (bWithUnrealMcp)
	{
		const FString CfgFile = EnsureMcpConfig();
		if (CfgFile.IsEmpty())
		{
			OnDone(false, FString(), TEXT("Could not write the MCP config file."));
			return;
		}
		ToolFlags = FString::Printf(TEXT(" --mcp-config \"%s\" --dangerously-skip-permissions"), *CfgFile);
	}

	const FString Claude = ClaudePath.IsEmpty() ? TEXT("claude") : ClaudePath;

	// Stream events (stream-json + verbose), force subscription auth (clear key),
	// feed the prompt via stdin, merge stderr so errors show in the log.
	const FString Params = FString::Printf(
		TEXT("/c set \"ANTHROPIC_API_KEY=\" && \"%s\" -p --output-format stream-json --verbose%s < \"%s\" 2>&1"),
		*Claude, *ToolFlags, *PromptFile);

	const TSharedRef<FClaudeRun> Ctx = MakeShared<FClaudeRun>();
	Ctx->OnProgress = MoveTemp(OnProgress);
	Ctx->OnDone = MoveTemp(OnDone);

	const TSharedPtr<FMonitoredProcess> Proc = MakeShared<FMonitoredProcess>(
		TEXT("cmd.exe"), Params, /*Hidden*/ true, /*CreatePipes*/ true);
	ActiveProc = Proc;

	Proc->OnOutput().BindLambda([Ctx](FString Chunk)
	{
		AsyncTask(ENamedThreads::GameThread, [Ctx, Chunk]()
		{
			Ctx->LineBuffer += Chunk;
			DrainBuffer(*Ctx);
		});
	});

	TWeakPtr<FClaudeBridge> WeakThis = AsShared();
	Proc->OnCompleted().BindLambda([WeakThis, Ctx](int32 ReturnCode)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Ctx, ReturnCode]()
		{
			FlushBuffer(*Ctx);
			const bool bOk = (ReturnCode == 0);
			Ctx->OnDone(bOk, Ctx->FinalOutput.TrimEnd(),
				bOk ? FString() : FString::Printf(TEXT("claude exited %d (see log)"), ReturnCode));

			if (const TSharedPtr<FClaudeBridge> Self = WeakThis.Pin())
			{
				Self->ActiveProc.Reset();
			}
		});
	});

	Proc->Launch();
}
