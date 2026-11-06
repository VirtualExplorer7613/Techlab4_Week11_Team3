#include "EnginePCH.h"
#include "Components/AudioComponent.h"
#include "Audio/AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Asset/AssetManager.h"
#include "Engine/Classes/Sound/SoundWave.h"

uint64 UAudioComponent::AudioComponentIDCounter = 0;
TMap<uint64, UAudioComponent*> UAudioComponent::AudioIDToComponentMap;

static FAudioDevice* GetAudioDevice() { return GEngine ? GEngine->GetMainAudioDevice() : nullptr; }

UAudioComponent::UAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;   // 재생 중 위치 갱신
	AudioComponentID = ++AudioComponentIDCounter;
	AudioIDToComponentMap[AudioComponentID] = this;
}

UAudioComponent::~UAudioComponent()
{
	if (FAudioDevice* AudioDevice = GetAudioDevice()) AudioDevice->StopActiveSound(AudioComponentID);
	AudioIDToComponentMap.Remove(AudioComponentID);
}

void UAudioComponent::SetSound(USoundBase* NewSound)
{
	const bool bWasPlaying = IsPlaying();
	Stop();
	Sound = NewSound;
	if (USoundWave* Wave = Cast<USoundWave>(NewSound)) SoundPath = Wave->SourcePath;
	if (bWasPlaying) Play();
}

USoundBase* UAudioComponent::GetSound()
{
	if (!Sound && !SoundPath.empty()) Sound = UAssetManager::Get().LoadSound(SoundPath);
	return Sound;
}

FActiveSound UAudioComponent::MakeActiveSound(float StartTime) const
{
	FActiveSound NewActiveSound;
	NewActiveSound.AudioComponentID = AudioComponentID;
	NewActiveSound.Sound = Sound;
	NewActiveSound.World = GetWorld();
	NewActiveSound.Location = GetWorldLocation();
	NewActiveSound.VolumeMultiplier = VolumeMultiplier;
	NewActiveSound.PitchMultiplier = PitchMultiplier;
	NewActiveSound.RequestedStartTime = StartTime;
	NewActiveSound.bLooping = bLooping;
	NewActiveSound.bIsUISound = bIsUISound;
	NewActiveSound.bHasAttenuationSettings = bAllowSpatialization;
	NewActiveSound.AttenuationSettings.InnerRadius = AttenuationInnerRadius;
	NewActiveSound.AttenuationSettings.FalloffDistance = AttenuationFalloffDistance;
	return NewActiveSound;
}

// UE UAudioComponent::PlayInternal (AudioComponent.cpp:673)
void UAudioComponent::Play(float StartTime)
{
	FAudioDevice* AudioDevice = GetAudioDevice();
	if (!AudioDevice || !GetSound()) return;

	// 재생 중에 다시 Play하면 처음부터 (OnAudioFinished는 부르지 않음)
	AudioDevice->StopActiveSound(AudioComponentID);
	AudioDevice->AddNewActiveSound(MakeActiveSound(StartTime));
}

// UE UAudioComponent::Stop (AudioComponent.cpp:1067)
void UAudioComponent::Stop()
{
	FAudioDevice* AudioDevice = GetAudioDevice();
	if (!AudioDevice || !IsPlaying()) return;
	AudioDevice->StopActiveSound(AudioComponentID);
	OnAudioFinished.Broadcast();
}

bool UAudioComponent::IsPlaying() const
{
	FAudioDevice* AudioDevice = GetAudioDevice();
	return AudioDevice && AudioDevice->FindActiveSound(AudioComponentID) != nullptr;
}

void UAudioComponent::SetVolumeMultiplier(float NewVolumeMultiplier)
{
	VolumeMultiplier = NewVolumeMultiplier;
	if (FAudioDevice* AudioDevice = GetAudioDevice())
		if (FActiveSound* ActiveSound = AudioDevice->FindActiveSound(AudioComponentID))
			ActiveSound->VolumeMultiplier = NewVolumeMultiplier;
}

void UAudioComponent::SetPitchMultiplier(float NewPitchMultiplier)
{
	PitchMultiplier = NewPitchMultiplier;
	if (FAudioDevice* AudioDevice = GetAudioDevice())
		if (FActiveSound* ActiveSound = AudioDevice->FindActiveSound(AudioComponentID))
			ActiveSound->PitchMultiplier = NewPitchMultiplier;
}

void UAudioComponent::PlaybackCompleted(uint64 InAudioComponentID)
{
	UAudioComponent** Found = AudioIDToComponentMap.Find(InAudioComponentID);
	if (Found && *Found) (*Found)->OnAudioFinished.Broadcast();
}

// bAutoActivate(기본 true)면 BeginPlayComponent → Activate → Play. 에디터 월드에선 BeginPlay가 없으므로 울리지 않는다.
void UAudioComponent::Activate(bool bReset)
{
	if (!bReset && !ShouldActivate()) return;
	Super::Activate(bReset);
	Play();
}

void UAudioComponent::Deactivate()
{
	Stop();
	Super::Deactivate();
}

// UE는 OnUpdateTransform에서 장치로 보낸다. 여기선 Tick에서 위치만 갱신.
void UAudioComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (!bAllowSpatialization) return;
	if (FAudioDevice* AudioDevice = GetAudioDevice())
		if (FActiveSound* ActiveSound = AudioDevice->FindActiveSound(AudioComponentID))
			ActiveSound->Location = GetWorldLocation();
}

void UAudioComponent::OnUnregister()
{
	if (FAudioDevice* AudioDevice = GetAudioDevice()) AudioDevice->StopActiveSound(AudioComponentID);
	Super::OnUnregister();
}

void UAudioComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "SoundPath") Sound = nullptr;   // 다음 GetSound()에서 다시 로드
}