#pragma once
#include "Audio/AudioPlatform.h"
#include "Audio/MiniaudioInclude.h"

class FAudioPlatformMiniaudio : public IAudioPlatformInterface
{
public:
	~FAudioPlatformMiniaudio() override { CloseAudioStream(); }

	bool OpenAudioStream(const FAudioStreamParams& InParams, FAudioRenderCallback InCallback) override;
	bool StartAudioStream() override;
	void StopAudioStream() override;
	void CloseAudioStream() override;
	const FAudioStreamParams& GetStreamParams() const override { return Params; }

private:
	static void DataCallback(ma_device* InDevice, void* Output, const void* Input, ma_uint32 FrameCount);

	ma_device Device{};
	bool bOpened = false;
	FAudioStreamParams Params;
	FAudioRenderCallback Callback;
};