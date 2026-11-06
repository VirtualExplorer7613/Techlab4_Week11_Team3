#pragma once

#include "MovieScene/MovieSceneTrack.h"
#include "MovieSceneSlomoTrack.generated.h"

// UE UMovieSceneSlomoSection: FloatCurve = 월드 CinematicTimeDilation (기본 1)
UCLASS()
class UMovieSceneSlomoSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneSlomoSection();

	int32 GetNumChannels() const override { return 1; }
	FMovieSceneFloatChannel* GetChannel(int32 Index) override { return Index == 0 ? &FloatCurve : nullptr; }
	const char* GetChannelName(int32 Index) const override { return "Time Dilation"; }
	void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) override;

	FMovieSceneFloatChannel FloatCurve;
};

// UE UMovieSceneSlomoTrack. UE 슬로모 시스템은 WorldSettings->SetCinematicTimeDilation을 부른다
UCLASS()
class UMovieSceneSlomoTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieSceneSlomoTrack() = default;

	const char* GetDisplayName() const override { return "Play Rate"; }
	UMovieSceneSection* CreateNewSection() override;
};
