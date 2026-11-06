#pragma once
#include "Components/SceneComponent.h"
#include "Core/Delegate/Delegate.h"

#include "Audio/ActiveSound.h"
#include "AudioComponent.generated.h"

class USoundBase;

DECLARE_DELEGATE(FOnAudioFinished);

UCLASS()
class UAudioComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAudioComponent();
	~UAudioComponent() override;

	void SetSound(USoundBase* NewSound);
	USoundBase* GetSound();

	virtual void Play(float StartTime = 0.0f);
	virtual void Stop();
	bool IsPlaying() const;

	void SetVolumeMultiplier(float NewVolumeMultiplier);
	void SetPitchMultiplier(float NewPitchMultiplier);

	// 자연 종료와 Stop() 모두에서 불린다 (UE와 같음).
	FOnAudioFinished OnAudioFinished;

	// FAudioDevice가 부른다.
	static void PlaybackCompleted(uint64 AudioComponentID);

	void Activate(bool bReset = false) override;
	void Deactivate() override;
	void TickComponent(float DeltaTime) override;
	void OnUnregister() override;
	void PostEditChangeProperty(const FProperty& Property) override;

	UPROPERTY(EditAnywhere)
	FString SoundPath;
	UPROPERTY(EditAnywhere)
	float VolumeMultiplier = 1.0f;
	UPROPERTY(EditAnywhere)
	float PitchMultiplier = 1.0f;
	UPROPERTY(EditAnywhere)
	bool bLooping = false;              // UE에서는 USoundWave::bLooping. 메타데이터 파일이 없어 컴포넌트에 둔다.
	UPROPERTY(EditAnywhere)
	bool bIsUISound = false;
	UPROPERTY(EditAnywhere)
	bool bAllowSpatialization = true;   // false면 2D
	UPROPERTY(EditAnywhere)
	float AttenuationInnerRadius = 400.0f;
	UPROPERTY(EditAnywhere)
	float AttenuationFalloffDistance = 3600.0f;

private:
	FActiveSound MakeActiveSound(float StartTime) const;

	USoundBase* Sound = nullptr;
	uint64 AudioComponentID = 0;

	static uint64 AudioComponentIDCounter;
	static TMap<uint64, UAudioComponent*> AudioIDToComponentMap;
};