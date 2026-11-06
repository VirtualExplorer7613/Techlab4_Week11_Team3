#pragma once
#include "Engine/Classes/Sound/SoundBase.h"
#include "SoundWave.generated.h"

UCLASS()
class USoundWave : public USoundBase
{
	GENERATED_BODY()
public:
	bool LoadFromFile(const FString& Path);

	float GetDuration() const override { return Duration; }

	// 오디오 스레드가 읽는다. 로드 후에는 바꾸지 않는다.
	TArray<float> PCMData;      // interleaved
	uint32 NumChannels = 0;     // 1 또는 2
	uint32 SampleRate = 0;      // 원본 샘플레이트. 장치와 다르면 믹서가 비율로 리샘플한다.
	uint64 NumFrames = 0;
	float Duration = 0.0f;
	FString SourcePath;
};