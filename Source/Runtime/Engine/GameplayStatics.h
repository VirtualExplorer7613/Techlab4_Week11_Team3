#pragma once

#include "Core/EngineString.h"

class UWorld;
class UGameInstance;
class USoundBase;
struct FSoundAttenuationSettings;

struct UGameplayStatics
{
	static void PlaySound2D(const UWorld* World, USoundBase* Sound, float VolumeMultiplier = 1.0f,
		float PitchMultiplier = 1.0f, float StartTime = 0.0f, bool bIsUISound = true);

	static void PlaySoundAtLocation(const UWorld* World, USoundBase* Sound, FVector Location,
		float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f, float StartTime = 0.0f,
		const FSoundAttenuationSettings* AttenuationSettings = nullptr);

	static void OpenLevel(UWorld* World, const FString& LevelName);
	static UGameInstance* GetGameInstance(const UWorld* World);
	static FString ResolveLevelPath(const FString& LevelName);
};