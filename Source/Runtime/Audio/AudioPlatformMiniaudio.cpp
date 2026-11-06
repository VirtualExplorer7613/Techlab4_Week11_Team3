#include "EnginePCH.h"
#include "AudioPlatformMiniaudio.h"

bool FAudioPlatformMiniaudio::OpenAudioStream(const FAudioStreamParams& InParams, FAudioRenderCallback InCallback)
{
	Params = InParams;
	Callback = std::move(InCallback);

	ma_device_config Config = ma_device_config_init(ma_device_type_playback);
	Config.playback.format = ma_format_f32;
	Config.playback.channels = Params.NumChannels;
	Config.sampleRate = Params.SampleRate;
	Config.periodSizeInFrames = Params.FramesPerBuffer;
	Config.dataCallback = &FAudioPlatformMiniaudio::DataCallback;
	Config.pUserData = this;

	if (ma_device_init(nullptr, &Config, &Device) != MA_SUCCESS)
	{
		HTR_LOG(Error, "[Audio] Failed to open playback device");
		return false;
	}

	// 공유 모드에서는 요청과 다른 값이 올 수 있다.
	Params.SampleRate = Device.sampleRate;
	Params.NumChannels = Device.playback.channels;
	bOpened = true;
	return true;
}

bool FAudioPlatformMiniaudio::StartAudioStream()
{
	return bOpened && ma_device_start(&Device) == MA_SUCCESS;
}

void FAudioPlatformMiniaudio::StopAudioStream()
{
	if (bOpened) ma_device_stop(&Device);
}

void FAudioPlatformMiniaudio::CloseAudioStream()
{
	if (!bOpened) return;
	ma_device_uninit(&Device);   // 내부에서 오디오 스레드가 끝날 때까지 기다린다
	bOpened = false;
}

void FAudioPlatformMiniaudio::DataCallback(ma_device* InDevice, void* Output, const void*, ma_uint32 FrameCount)
{
	// miniaudio는 기본 설정에서 Output을 0으로 채워서 넘긴다 (noPreSilencedOutputBuffer = false).
	auto* Self = static_cast<FAudioPlatformMiniaudio*>(InDevice->pUserData);
	if (Self->Callback)
		Self->Callback(static_cast<float*>(Output), FrameCount, Self->Params.NumChannels);
}