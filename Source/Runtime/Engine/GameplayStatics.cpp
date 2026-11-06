#include "EnginePCH.h"
#include "GameplayStatics.h"

#include "Audio/AudioDevice.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

FString UGameplayStatics::ResolveLevelPath(const FString& LevelName)
{
	// 경로가 들어 있으면 그대로 쓴다.
	if (LevelName.find('/') != FString::npos || LevelName.find('\\') != FString::npos)
		return LevelName;
	constexpr const char* Ext = ".scene";
	const bool bHasExt = LevelName.size() >= 6 && LevelName.compare(LevelName.size() - 6, 6, Ext) == 0;
	return "Scenes/" + LevelName + (bHasExt ? "" : Ext);
}


void UGameplayStatics::OpenLevel(UWorld* World, const FString& LevelName)
{
	if (!GEngine || !World || LevelName.empty()) return;
	GEngine->SetClientTravel(World, ResolveLevelPath(LevelName));
}

UGameInstance* UGameplayStatics::GetGameInstance(const UWorld* World)
{
	FWorldContext* Context = GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	return Context ? Context->OwningGameInstance : nullptr;
}


void UGameplayStatics::PlaySound2D(const UWorld* World, USoundBase* Sound, float VolumeMultiplier,
	float PitchMultiplier, float StartTime, bool bIsUISound)
{
	FAudioDevice* AudioDevice = GEngine ? GEngine->GetMainAudioDevice() : nullptr;
	if (!AudioDevice || !Sound) return;

	FActiveSound NewActiveSound;
	NewActiveSound.Sound = Sound;
	NewActiveSound.World = const_cast<UWorld*>(World);
	NewActiveSound.VolumeMultiplier = VolumeMultiplier;
	NewActiveSound.PitchMultiplier = PitchMultiplier;
	NewActiveSound.RequestedStartTime = StartTime;
	NewActiveSound.bIsUISound = bIsUISound;
	AudioDevice->AddNewActiveSound(NewActiveSound);
}

void UGameplayStatics::PlaySoundAtLocation(const UWorld* World, USoundBase* Sound, FVector Location,
	float VolumeMultiplier, float PitchMultiplier, float StartTime, const FSoundAttenuationSettings* AttenuationSettings)
{
	FAudioDevice* AudioDevice = GEngine ? GEngine->GetMainAudioDevice() : nullptr;
	if (!AudioDevice || !Sound) return;

	FActiveSound NewActiveSound;
	NewActiveSound.Sound = Sound;
	NewActiveSound.World = const_cast<UWorld*>(World);
	NewActiveSound.Location = Location;
	NewActiveSound.VolumeMultiplier = VolumeMultiplier;
	NewActiveSound.PitchMultiplier = PitchMultiplier;
	NewActiveSound.RequestedStartTime = StartTime;
	NewActiveSound.bHasAttenuationSettings = true;
	if (AttenuationSettings) NewActiveSound.AttenuationSettings = *AttenuationSettings;
	AudioDevice->AddNewActiveSound(NewActiveSound);
}