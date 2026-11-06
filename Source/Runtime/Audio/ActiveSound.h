#pragma once
#include "Audio/SoundAttenuation.h"
#include "Math/EngineMath.h"

class USoundBase;
class USoundWave;
class UWorld;

struct FListener
{
	UWorld* World = nullptr;
	FVector Location = FVector::ZeroVector;
	FVector Front = FVector(1, 0, 0);
	FVector Right = FVector(0, 1, 0);
};

struct FActiveSound
{
	uint64 AudioComponentID = 0;    // 0 = 컴포넌트 없는 fire-and-forget (PlaySound2D 등)
	USoundBase* Sound = nullptr;
	UWorld* World = nullptr;

	FVector Location = FVector::ZeroVector;
	bool bHasAttenuationSettings = false;   // false면 2D
	FSoundAttenuationSettings AttenuationSettings;

	float VolumeMultiplier = 1.0f;
	float PitchMultiplier = 1.0f;
	float RequestedStartTime = 0.0f;
	bool bLooping = false;
	bool bIsUISound = false;        // 게임 일시정지 중에도 재생

	// 장치 내부 상태
	USoundWave* Wave = nullptr;
	uint32 SourceId = 0;
	bool bFinished = false;
};
