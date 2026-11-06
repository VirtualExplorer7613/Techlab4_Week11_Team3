#include "EnginePCH.h"
#include "AudioMixer.h"
#include "Engine/Classes/Sound/SoundWave.h"
#include <algorithm>
#include <cmath>

bool FAudioMixer::Init(TUniquePtr<IAudioPlatformInterface> InPlatform, const FAudioStreamParams& Params)
{
	Platform = std::move(InPlatform);
	const bool bOpened = Platform->OpenAudioStream(Params,
		[this](float* Out, uint32 NumFrames, uint32 NumChannels) { OnRender(Out, NumFrames, NumChannels); });
	if (!bOpened) return false;

	DeviceSampleRate = Platform->GetStreamParams().SampleRate;
	return Platform->StartAudioStream();
}

void FAudioMixer::Shutdown()
{
	if (!Platform) return;
	Platform->StopAudioStream();
	Platform->CloseAudioStream();     // 이 뒤로 OnRender는 불리지 않는다
	Platform.reset();
	Sources.Reset();
	PendingCommands.Reset();
}

void FAudioMixer::EnqueueCommand(std::function<void()> Command)
{
	std::lock_guard<std::mutex> Lock(CommandMutex);
	PendingCommands.Add(std::move(Command));
}

uint32 FAudioMixer::CreateSource(const USoundWave* Wave, const FMixerSourceParams& Params, float StartTime)
{
	const uint32 SourceId = NextSourceId.fetch_add(1);
	EnqueueCommand([this, SourceId, Wave, Params, StartTime]()
		{
			FMixerSource& Source = Sources.AddDefaulted_GetRef();
			Source.SourceId = SourceId;
			Source.Wave = Wave;
			Source.Params = Params;
			Source.Cursor = FMath::Clamp(static_cast<double>(StartTime) * Wave->SampleRate, 0.0, static_cast<double>(Wave->NumFrames));
		});
	return SourceId;
}

void FAudioMixer::UpdateSource(uint32 SourceId, const FMixerSourceParams& Params)
{
	EnqueueCommand([this, SourceId, Params]()
		{
			if (FMixerSource* Source = FindSource(SourceId)) Source->Params = Params;
		});
}

void FAudioMixer::StopSource(uint32 SourceId)
{
	EnqueueCommand([this, SourceId]()
		{
			if (FMixerSource* Source = FindSource(SourceId)) Source->bStopping = true;
		});
}

void FAudioMixer::StopAllSources()
{
	EnqueueCommand([this]() { for (FMixerSource& Source : Sources) Source.bStopping = true; });
}

void FAudioMixer::PumpFinishedSources(TArray<uint32>& OutIds)
{
	std::lock_guard<std::mutex> Lock(FinishedMutex);
	OutIds.Swap(FinishedSourceIds);
	FinishedSourceIds.Reset();
}

FAudioMixer::FMixerSource* FAudioMixer::FindSource(uint32 SourceId)
{
	for (FMixerSource& Source : Sources)
		if (Source.SourceId == SourceId) return &Source;
	return nullptr;
}

void FAudioMixer::ProcessCommands()
{
	{
		std::lock_guard<std::mutex> Lock(CommandMutex);
		ExecutingCommands.Swap(PendingCommands);
	}
	for (std::function<void()>& Command : ExecutingCommands) Command();
	ExecutingCommands.Reset();
}

// UE FMixerDevice::OnProcessAudioStream → FMixerSourceManager::ComputeNextBlockOfSamples 흐름을 한 함수로.
void FAudioMixer::OnRender(float* Out, uint32 NumFrames, uint32 NumChannels)
{
	ProcessCommands();

	for (size_t Index = 0; Index < Sources.size();)
	{
		if (MixSource(Sources[Index], Out, NumFrames, NumChannels))
		{
			{
				std::lock_guard<std::mutex> Lock(FinishedMutex);
				FinishedSourceIds.Add(Sources[Index].SourceId);
			}
			Sources[Index] = std::move(Sources.Last());
			Sources.RemoveLast();
		}
		else
		{
			++Index;
		}
	}

	// 마스터 볼륨 + 하드 클립. (UE는 서브믹스 체인 끝에서 처리)
	const float Master = MasterVolume.load();
	const uint32 NumSamples = NumFrames * NumChannels;
	for (uint32 Sample = 0; Sample < NumSamples; ++Sample)
		Out[Sample] = std::clamp(Out[Sample] * Master, -1.0f, 1.0f);
}

bool FAudioMixer::MixSource(FMixerSource& Source, float* Out, uint32 NumFrames, uint32 NumChannels)
{
	if (Source.Params.bPaused && !Source.bStopping) return false;

	const USoundWave* Wave = Source.Wave;
	const float* Pcm = Wave->PCMData.GetData();
	const uint64 WaveFrames = Wave->NumFrames;
	const uint32 SrcChannels = Wave->NumChannels;

	const float TargetL = Source.bStopping ? 0.0f : Source.Params.GainL * Source.Params.Volume;
	const float TargetR = Source.bStopping ? 0.0f : Source.Params.GainR * Source.Params.Volume;
	if (Source.bFirstBlock)
	{
		Source.CurGainL = TargetL;
		Source.CurGainR = TargetR;
		Source.bFirstBlock = false;
	}
	const float StepL = (TargetL - Source.CurGainL) / NumFrames;
	const float StepR = (TargetR - Source.CurGainR) / NumFrames;

	// 피치 × (원본 샘플레이트 / 장치 샘플레이트) 만큼 원본 커서를 전진 → 선형 보간 리샘플링
	const double Step = std::max(0.01f, Source.Params.Pitch) * static_cast<double>(Wave->SampleRate) / DeviceSampleRate;

	for (uint32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		if (Source.Cursor >= WaveFrames)
		{
			if (!Source.Params.bLooping) return true;
			Source.Cursor = std::fmod(Source.Cursor, static_cast<double>(WaveFrames));
		}

		const uint64 I0 = static_cast<uint64>(Source.Cursor);
		uint64 I1 = I0 + 1;
		if (I1 >= WaveFrames) I1 = Source.Params.bLooping ? 0 : I0;
		const float T = static_cast<float>(Source.Cursor - static_cast<double>(I0));

		float Left, Right;
		if (SrcChannels == 1)
		{
			Left = Right = Pcm[I0] + (Pcm[I1] - Pcm[I0]) * T;
		}
		else
		{
			Left = Pcm[I0 * 2] + (Pcm[I1 * 2] - Pcm[I0 * 2]) * T;
			Right = Pcm[I0 * 2 + 1] + (Pcm[I1 * 2 + 1] - Pcm[I0 * 2 + 1]) * T;
		}

		Source.CurGainL += StepL;
		Source.CurGainR += StepR;

		float* OutFrame = Out + Frame * NumChannels;
		if (NumChannels == 1)
		{
			OutFrame[0] += 0.5f * (Left * Source.CurGainL + Right * Source.CurGainR);
		}
		else
		{
			OutFrame[0] += Left * Source.CurGainL;     // 3채널 이상 장치는 앞 두 채널만 쓴다
			OutFrame[1] += Right * Source.CurGainR;
		}

		Source.Cursor += Step;
	}

	return Source.bStopping;   // 정지 요청은 한 버퍼 페이드아웃 후 제거
}