#pragma once
#include "Audio/ActiveSound.h"
#include "Audio/AudioMixer.h"

class FAudioDevice
{
public:
	bool Init();
	void Teardown();

	// 월드 Tick 이후 매 프레임. bGameTicking=false면 UI 사운드만 재생.
	void Update(bool bGameTicking);

	void SetListener(UWorld* World, const FVector& Location, const FVector& Front, const FVector& Right);
	const FListener& GetListener() const { return Listener; }

	void AddNewActiveSound(const FActiveSound& NewActiveSound);
	FActiveSound* FindActiveSound(uint64 AudioComponentID);
	void StopActiveSound(uint64 AudioComponentID);

	// 월드 하나(또는 nullptr이면 전부)의 사운드를 즉시 정지. 월드 파괴·PIE 종료 시.
	void Flush(UWorld* WorldToFlush);
	// 에셋 해제 전에 그 에셋을 읽는 소스를 모두 정지.
	void StopSoundsUsingResource(USoundWave* SoundWave);

	void SetTransientPrimaryVolume(float Volume) { Mixer.SetMasterVolume(Volume); }
	uint32 GetSampleRate() const { return Mixer.GetSampleRate(); }

private:
	FMixerSourceParams ComputeSourceParams(const FActiveSound& ActiveSound, bool bGameTicking) const;
	void RemoveActiveSoundAt(int32 Index, bool bNotifyComponent);

	FAudioMixer Mixer;
	FListener Listener;
	TArray<FActiveSound> ActiveSounds;
	TArray<uint32> FinishedScratch;
	bool bInitialized = false;
}; 