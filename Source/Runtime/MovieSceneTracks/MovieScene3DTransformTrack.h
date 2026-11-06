#pragma once

#include "MovieScene/MovieSceneTrack.h"
#include "MovieScene3DTransformTrack.generated.h"

// UE UMovieScene3DTransformSection: Translation[3], Rotation[3], Scale[3]
UCLASS()
class UMovieScene3DTransformSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieScene3DTransformSection() = default;

	int32 GetNumChannels() const override { return 9; }
	FMovieSceneFloatChannel* GetChannel(int32 Index) override;
	const char* GetChannelName(int32 Index) const override;
	void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) override;
	void KeyCurrentValues(FFrameNumber Time, IMovieScenePlayer& Player, UObject* BoundObject) override;

	FMovieSceneFloatChannel Translation[3];
	FMovieSceneFloatChannel Rotation[3];   // Pitch, Yaw, Roll (도)
	FMovieSceneFloatChannel Scale[3];
};

// UE UMovieScene3DTransformTrack
UCLASS()
class UMovieScene3DTransformTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieScene3DTransformTrack() = default;

	const char* GetDisplayName() const override { return "Transform"; }
	bool IsObjectBindingTrack() const override { return true; }
	UMovieSceneSection* CreateNewSection() override;
};
