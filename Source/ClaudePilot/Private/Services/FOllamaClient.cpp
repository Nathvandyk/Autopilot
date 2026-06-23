#include "Services/FOllamaClient.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FOllamaClient::FOllamaClient(const FString& InBaseUrl, const FString& InModel)
	: BaseUrl(InBaseUrl)
	, Model(InModel)
{
}

void FOllamaClient::SetEndpoint(const FString& InBaseUrl, const FString& InModel)
{
	BaseUrl = InBaseUrl;
	Model = InModel;
}

void FOllamaClient::Chat(const FString& System, const FString& User,
	TFunction<void(bool, const FString&, const FString&)> OnDone)
{
	// Build the request body with the JSON DOM so quotes/newlines in the
	// prompts are escaped correctly.
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("model"), Model);
	Payload->SetBoolField(TEXT("stream"), false);
	Payload->SetStringField(TEXT("format"), TEXT("json"));

	TArray<TSharedPtr<FJsonValue>> Messages;
	{
		const TSharedRef<FJsonObject> Sys = MakeShared<FJsonObject>();
		Sys->SetStringField(TEXT("role"), TEXT("system"));
		Sys->SetStringField(TEXT("content"), System);
		Messages.Add(MakeShared<FJsonValueObject>(Sys));

		const TSharedRef<FJsonObject> Usr = MakeShared<FJsonObject>();
		Usr->SetStringField(TEXT("role"), TEXT("user"));
		Usr->SetStringField(TEXT("content"), User);
		Messages.Add(MakeShared<FJsonValueObject>(Usr));
	}
	Payload->SetArrayField(TEXT("messages"), Messages);

	const TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetNumberField(TEXT("temperature"), 0.2);   // low: we want obedient JSON
	Payload->SetObjectField(TEXT("options"), Options);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Payload, Writer);

	FString Url = BaseUrl;
	Url.RemoveFromEnd(TEXT("/"));
	Url += TEXT("/api/chat");

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Req->SetContentAsString(Body);
	Req->OnProcessRequestComplete().BindLambda(
		[OnDone](FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				OnDone(false, FString(), TEXT("Could not reach Ollama - is `ollama serve` running?"));
				return;
			}

			const int32 Code = Response->GetResponseCode();
			const FString Resp = Response->GetContentAsString();
			if (Code != 200)
			{
				OnDone(false, FString(),
					FString::Printf(TEXT("Ollama HTTP %d: %s"), Code, *Resp.Left(300)));
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
			if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
			{
				const TSharedPtr<FJsonObject>* Msg = nullptr;
				if (Root->TryGetObjectField(TEXT("message"), Msg) && Msg)
				{
					FString Content;
					(*Msg)->TryGetStringField(TEXT("content"), Content);
					OnDone(true, Content, FString());
					return;
				}
			}
			OnDone(false, FString(), TEXT("Unexpected Ollama response shape."));
		});
	Req->ProcessRequest();
}
