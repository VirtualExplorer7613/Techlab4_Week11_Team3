#include "EnginePCH.h"
#include "Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/IceVolumeComponent.h"
#include "Components/SlidingMovementComponent.h"
#include "Audio/AudioDevice.h"
#include "Components/AudioComponent.h"

#include "Serialization/JsonArchive.h"

UEngine* GEngine = nullptr;

bool UEngine::Init()
{
	// Keep reflected shape classes linked even when no actor references them yet.
	// Add Component and scene loading both discover these classes by registration.
	UBoxComponent::StaticClass();
	USphereComponent::StaticClass();
	UCapsuleComponent::StaticClass();
	UIceVolumeComponent::StaticClass();
	USlidingMovementComponent::StaticClass();
	UAudioComponent::StaticClass();   // Add Component 목록에 뜨도록 (다른 컴포넌트와 동일)


	AssetManager = NewObject<UAssetManager>();
	InitializeAudioDevice();          // 실패해도 엔진은 계속 (UE도 장치가 없으면 Null 디바이스로 돈다)
	/*World = FObjectFactory::ConstructObject<UWorld>();

	if (!World || !World->Init()) return false;*/

 	return true;
}

void UEngine::InitializeAudioDevice()
{
	MainAudioDevice = new FAudioDevice();
	if (!MainAudioDevice->Init())
	{
		HTR_LOG(Warning, "[Audio] No audio device. Sound disabled.");
		delete MainAudioDevice;
		MainAudioDevice = nullptr;
	}
}

void UEngine::ShutdownAudioDevice()
{
	if (!MainAudioDevice) return;
	MainAudioDevice->Teardown();      // 오디오 스레드 종료. 이후 USoundWave를 지워도 안전
	delete MainAudioDevice;
	MainAudioDevice = nullptr;
}

FWorldContext& UEngine::CreateNewWorldContext(EWorldType WorldType)
{
	FWorldContext* NewWorldContext = new FWorldContext();
	WorldContextlist.Add(NewWorldContext);
	NewWorldContext->WorldType = WorldType;
	NewWorldContext->ContextHandle = FName();

	return *NewWorldContext;
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld)
{
	for (FWorldContext& WorldContext : WorldContextlist)
	{
		if (WorldContext.World() == InWorld)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromType(EWorldType WorldType)
{
	for (FWorldContext& Context : WorldContextlist)
		if (Context.WorldType == WorldType) return &Context;
	return nullptr;
}

FWorldContext* UEngine::GetWorldContextFromPIEInstance(int32 PIEInstance)
{
	for (FWorldContext& Context : WorldContextlist)
		if (Context.WorldType == EWorldType::PIE && Context.PIEInstance == PIEInstance) return &Context;
	return nullptr;
}

UWorld* UEngine::GetEditorWorld() const
{
	for (const FWorldContext& Context : WorldContextlist)
		if (Context.WorldType == EWorldType::Editor) return Context.World();
	return nullptr;
}

UWorld* UEngine::CreatePIEWorldByDuplication(FWorldContext& Context, UWorld* SourceWorld)
{
	UWorld* World = UWorld::GetDuplicatedWorldForPIE(SourceWorld);
	if (World) Context.SetCurrentWorld(World);
	return World;
}

void UEngine::DestroyWorldContext(UWorld* InWorld)
{
	for (int32 idx = 0; idx < WorldContextlist.Num(); ++idx)
	{
		if (WorldContextlist[idx].World() == InWorld)
		{
			const EWorldType ContextType = WorldContextlist[idx].WorldType;
			if (WorldContextlist[idx].GameViewport)
			{
				WorldContextlist[idx].GameViewport->LostFocus();
				WorldContextlist[idx].GameViewport->Reset();
			}
			if (WorldContextlist[idx].OwningGameInstance)
			{
				UGameInstance* Instance = WorldContextlist[idx].OwningGameInstance;
				Instance->Shutdown();
				delete Instance;
			}
			WorldContextlist[idx].SetCurrentWorld(nullptr);
			WorldContextlist.RemoveAt(idx);
			if (ContextType == EWorldType::PIE || ContextType == EWorldType::Game)
			{
				if (MainAudioDevice) MainAudioDevice->Flush(InWorld);
				DestroyWorld(InWorld);
			}
			break;
		}
	}
}

void UEngine::DestroyWorld(UWorld* InWorld)
{
	if (!InWorld) return;
	ACameraActor* Camera = InWorld->GetMainCamera();
	const bool bOwnsCamera = Camera && !Camera->GetLevel();
	InWorld->SetMainCamera(nullptr);
	InWorld->ClearWorld();
	for (ULevel* Level : InWorld->GetLevels()) delete Level;
	if (bOwnsCamera) delete Camera;
	delete InWorld;
}

void UEngine::SetClientTravel(UWorld* InWorld, const FString& MapPath)
{
	if (FWorldContext* Context = GetWorldContextFromWorld(InWorld))
		Context->TravelURL = MapPath;
}

void UEngine::TickWorldTravel(FWorldContext& Context)
{
	if (Context.TravelURL.empty()) return;
	const FString MapPath = Context.TravelURL;
	Context.TravelURL.clear();
	LoadMap(Context, MapPath);
}

bool UEngine::LoadMap(FWorldContext& Context, const FString& MapPath)
{
	// 새 월드를 먼저 읽는다. 파일이 없거나 깨졌으면 지금 월드를 그대로 둔다.
	UWorld* NewWorld = UWorld::CreateWorld(Context.WorldType, true);
	if (!NewWorld) return false;
	if (!FJsonArchive::LoadWorld(NewWorld, MapPath))
	{
		HTR_LOG(Error, "Failed to load map: {}", MapPath);
		DestroyWorld(NewWorld);
		return false;
	}

	PreLoadMap(Context);
	if (Context.GameViewport)
	{
		Context.GameViewport->ClearViewportWidgets();
		Context.GameViewport->SetWorld(nullptr);
	}
	
	DestroyWorld(Context.World());

	Context.SetCurrentWorld(NewWorld);
	if (Context.GameViewport)
		Context.GameViewport->SetWorld(NewWorld);
	NewWorld->BeginPlay();
	PostLoadMap(Context);

	HTR_LOG(Info, "Map loaded: {}", MapPath);
	return true;
}

void FWorldContext::SetCurrentWorld(UWorld* World)
{
	ThisCurrentWorld = World;
}
