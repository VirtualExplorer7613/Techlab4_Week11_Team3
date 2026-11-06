#pragma once

#include "MovieScene/MovieSceneTrack.h"
#include "MovieSceneTracks/MovieScenePropertyBinding.h"
#include "MovieSceneFloatTrack.generated.h"

// UE UMovieSceneFloatSection
UCLASS()
class UMovieSceneFloatSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneFloatSection() = default;

	int32 GetNumChannels() const override { return 1; }
	FMovieSceneFloatChannel* GetChannel(int32 Index) override { return Index == 0 ? &FloatCurve : nullptr; }
	const char* GetChannelName(int32 Index) const override;
	void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) override;
	void KeyCurrentValues(FFrameNumber Time, IMovieScenePlayer& Player, UObject* BoundObject) override;

	FMovieSceneFloatChannel FloatCurve;
};

// UE UMovieScenePropertyTrack: 바인딩된 오브젝트의 프로퍼티 하나를 움직인다
UCLASS()
class UMovieScenePropertyTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieScenePropertyTrack() = default;

	bool IsObjectBindingTrack() const override { return true; }
	const char* GetDisplayName() const override { return PropertyBinding.PropertyName.c_str(); }
	void SetPropertyNameAndPath(const FString& InPropertyName) { PropertyBinding.PropertyName = InPropertyName; }
	const FMovieScenePropertyBinding& GetPropertyBinding() const { return PropertyBinding; }

	void SaveJson(nlohmann::json& Out) const override;
	void LoadJson(const nlohmann::json& In) override;

protected:
	FMovieScenePropertyBinding PropertyBinding;
};

// UE UMovieSceneFloatTrack. 예: 카메라 FieldOfView
UCLASS()
class UMovieSceneFloatTrack : public UMovieScenePropertyTrack
{
	GENERATED_BODY()

public:
	UMovieSceneFloatTrack() = default;

	UMovieSceneSection* CreateNewSection() override;
};
