#pragma once
#include "Audio/AudioPlatform.h"
#include "Containers/Array.h"
#include <atomic>
#include <mutex>
#include <vector>

class USoundWave;

// 게임 스레드가 매 프레임 계산해서 넘기는 값. UE FWaveInstance의 Volume/Pitch/공간화 결과에 해당.
struct FMixerSourceParams
{
	float Volume = 1.0f;
	float Pitch = 1.0f;
	float GainL = 1.0f;     // 패닝 결과
	float GainR = 1.0f;
	bool bLooping = false;
	bool bPaused = false;
};

class FAudioMixer
{
public:
	bool Init(TUniquePtr<IAudioPlatformInterface> InPlatform, const FAudioStreamParams& Params);
	void Shutdown();

	// ---- 게임 스레드 API ----
	uint32 CreateSource(const USoundWave* Wave, const FMixerSourceParams& Params, float StartTime);
	void UpdateSource(uint32 SourceId, const FMixerSourceParams& Params);
	void StopSource(uint32 SourceId);
	void StopAllSources();
	void SetMasterVolume(float Volume) { MasterVolume.store(Volume); }
	// 오디오 스레드에서 끝난(자연 종료 + 정지 완료) 소스 ID를 꺼내 간다.
	void PumpFinishedSources(TArray<uint32>& OutIds);

	uint32 GetSampleRate() const { return DeviceSampleRate; }

private:
	struct FMixerSource
	{
		uint32 SourceId = 0;
		const USoundWave* Wave = nullptr;
		double Cursor = 0.0;            // 원본 프레임 단위 재생 위치
		FMixerSourceParams Params;
		float CurGainL = 0.0f, CurGainR = 0.0f;
		bool bFirstBlock = true;
		bool bStopping = false;
	};

	// ---- 오디오 스레드 ----
	void OnRender(float* Out, uint32 NumFrames, uint32 NumChannels);
	void ProcessCommands();
	bool MixSource(FMixerSource& Source, float* Out, uint32 NumFrames, uint32 NumChannels);  // true = 끝남
	FMixerSource* FindSource(uint32 SourceId);

	void EnqueueCommand(std::function<void()> Command);

	TUniquePtr<IAudioPlatformInterface> Platform;
	uint32 DeviceSampleRate = 48000;

	std::mutex CommandMutex;
	TArray<std::function<void()>> PendingCommands;     // 게임 -> 오디오
	TArray<std::function<void()>> ExecutingCommands;   // 오디오 스레드 전용 (할당 재사용)

	std::mutex FinishedMutex;
	TArray<uint32> FinishedSourceIds;                   // 오디오 -> 게임

	TArray<FMixerSource> Sources;                       // 오디오 스레드 전용
	std::atomic<uint32> NextSourceId{ 1 };
	std::atomic<float> MasterVolume{ 1.0f };
};