#pragma once
#include <functional>
#include "Core/Types.h"

struct FAudioStreamParams
{
	uint32 SampleRate = 48000;
	uint32 NumChannels= 2;
	uint32 FramesPerBuffer= 512;
};

using FAudioRenderCallback  = std::function<void(float* Out, uint32 NumFrames, uint32 NumChannels)>;

class IAudioPlatformInterface
{
public:
	virtual ~IAudioPlatformInterface() = default;

	virtual bool OpenAudioStream(const FAudioStreamParams& Params, FAudioRenderCallback Callback) = 0;
	virtual bool StartAudioStream() = 0;
	virtual void StopAudioStream() = 0;
	virtual void CloseAudioStream() = 0;

	virtual const FAudioStreamParams& GetStreamParams() const = 0;
};

