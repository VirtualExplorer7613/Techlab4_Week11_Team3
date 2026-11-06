#include "EnginePCH.h"
#include "Audio/AudioDevice.h"
#include "Audio/AudioPlatformMiniaudio.h"
#include "Components/AudioComponent.h"
#include "Engine/Classes/Sound/SoundWave.h"
#include <cmath>

bool FAudioDevice::Init()
{
	bInitialized = Mixer.Init(MakeUnique<FAudioPlatformMiniaudio>(), FAudioStreamParams{});
	if (bInitialized)
		HTR_LOG(Info, "[Audio] Device opened ({} Hz)", Mixer.GetSampleRate());
	return bInitialized;
}

void FAudioDevice::Teardown()
{
	Mixer.Shutdown();
	ActiveSounds.Empty();
	bInitialized = false;
}

void FAudioDevice::SetListener(UWorld* World, const FVector& Location, const FVector& Front, const FVector& Right)
{
	Listener.World = World;
	Listener.Location = Location;
	Listener.Front = Front;
	Listener.Right = Right;
}

void FAudioDevice::AddNewActiveSound(const FActiveSound& NewActiveSound)
{
	if (!bInitialized) return;
	USoundWave* Wave = Cast<USoundWave>(NewActiveSound.Sound);
	if (!Wave || Wave->NumFrames == 0) return;

	FActiveSound& Added = ActiveSounds.Emplace_GetRef(NewActiveSound);   // 엔진 TArray에 맞게: Add 후 Last()
	Added.Wave = Wave;
	Added.SourceId = Mixer.CreateSource(Wave, ComputeSourceParams(Added, true), Added.RequestedStartTime);
}

FActiveSound* FAudioDevice::FindActiveSound(uint64 AudioComponentID)
{
	for (FActiveSound& ActiveSound : ActiveSounds)
		if (ActiveSound.AudioComponentID == AudioComponentID && !ActiveSound.bFinished) return &ActiveSound;
	return nullptr;
}

void FAudioDevice::StopActiveSound(uint64 AudioComponentID)
{
	for (int32 Index = ActiveSounds.Num() - 1; Index >= 0; --Index)
		if (ActiveSounds[Index].AudioComponentID == AudioComponentID)
			RemoveActiveSoundAt(Index, false);
}

void FAudioDevice::Flush(UWorld* WorldToFlush)
{
	for (int32 Index = ActiveSounds.Num() - 1; Index >= 0; --Index)
		if (!WorldToFlush || ActiveSounds[Index].World == WorldToFlush)
			RemoveActiveSoundAt(Index, false);
	if (Listener.World == WorldToFlush) Listener.World = nullptr;
}

void FAudioDevice::StopSoundsUsingResource(USoundWave* SoundWave)
{
	for (int32 Index = ActiveSounds.Num() - 1; Index >= 0; --Index)
		if (ActiveSounds[Index].Wave == SoundWave)
			RemoveActiveSoundAt(Index, true);
	// 주의: 믹서는 다음 버퍼에서야 소스를 놓는다. 에셋 해제는 Teardown 이후에만 한다.
}

void FAudioDevice::RemoveActiveSoundAt(int32 Index, bool bNotifyComponent)
{
	const FActiveSound Removed = ActiveSounds[Index];
	if (!Removed.bFinished) Mixer.StopSource(Removed.SourceId);
	ActiveSounds.RemoveAtSwap(Index);
	if (bNotifyComponent && Removed.AudioComponentID != 0)
		UAudioComponent::PlaybackCompleted(Removed.AudioComponentID);
}

// UE FAudioDevice::Update (AudioDevice.cpp:4725): 끝난 소스 수거 → 리스너 기준으로 각 사운드 재계산 → 믹서로 전송
void FAudioDevice::Update(bool bGameTicking)
{
	if (!bInitialized) return;

	Mixer.PumpFinishedSources(FinishedScratch);
	for (uint32 SourceId : FinishedScratch)
		for (FActiveSound& ActiveSound : ActiveSounds)
			if (ActiveSound.SourceId == SourceId) { ActiveSound.bFinished = true; break; }

	// 뒤에서부터: PlaybackCompleted 안에서 다시 Play()해서 끝에 추가돼도 안전하다.
	for (int32 Index = ActiveSounds.Num() - 1; Index >= 0; --Index)
	{
		if (Index >= ActiveSounds.Num()) continue;
		FActiveSound& ActiveSound = ActiveSounds[Index];
		if (ActiveSound.bFinished)
		{
			RemoveActiveSoundAt(Index, true);
			continue;
		}
		Mixer.UpdateSource(ActiveSound.SourceId, ComputeSourceParams(ActiveSound, bGameTicking));
	}
}

FMixerSourceParams FAudioDevice::ComputeSourceParams(const FActiveSound& ActiveSound, bool bGameTicking) const
{
	FMixerSourceParams Params;
	Params.Volume = ActiveSound.VolumeMultiplier;
	Params.Pitch = ActiveSound.PitchMultiplier;
	Params.bLooping = ActiveSound.bLooping;
	Params.bPaused = !bGameTicking && !ActiveSound.bIsUISound;

	if (!ActiveSound.bHasAttenuationSettings) return Params;   // 2D: GainL = GainR = 1

	// 리스너가 다른 월드(에디터 월드 vs PIE 월드)에 있으면 3D 사운드는 들리지 않는다.
	if (Listener.World != ActiveSound.World)
	{
		Params.Volume = 0.0f;
		return Params;
	}

	const FSoundAttenuationSettings& Settings = ActiveSound.AttenuationSettings;
	const FVector ToSound = ActiveSound.Location - Listener.Location;
	const float Distance = ToSound.Size();

	Params.Volume *= Settings.Evaluate(Distance);

	if (Settings.bSpatialize)
	{
		// 등전력 패닝: Pan -1(왼쪽) ~ +1(오른쪽) → 각도 0 ~ π/2
		const float Pan = Distance > 1.0e-3f ? std::clamp(FVector::Dot(ToSound / Distance, Listener.Right), -1.0f, 1.0f) : 0.0f;
		const float Angle = (Pan + 1.0f) * 0.25f * 3.14159265f;
		Params.GainL = std::cos(Angle);
		Params.GainR = std::sin(Angle);
	}
	return Params;
}