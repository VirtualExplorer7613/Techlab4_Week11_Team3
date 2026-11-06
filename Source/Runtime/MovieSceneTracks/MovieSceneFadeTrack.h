#pragma once

#include "MovieScene/MovieSceneTrack.h"
#include "Math/Color.h"
#include "MovieSceneFadeTrack.generated.h"

// UE UMovieSceneFadeSection: FloatCurve(0 = 투명, 1 = FadeColor로 덮음), FadeColor, bFadeAudio
UCLASS()
class UMovieSceneFadeSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneFadeSection() = default;

	int32 GetNumChannels() const override { return 1; }
	FMovieSceneFloatChannel* GetChannel(int32 Index) override { return Index == 0 ? &FloatCurve : nullptr; }
	const char* GetChannelName(int32 Index) const override { return "Fade"; }
	void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) override;
	void SaveJson(nlohmann::json& Out) const override;
	void LoadJson(const nlohmann::json& In) override;

	FMovieSceneFloatChannel FloatCurve;
	FLinearColor FadeColor{ 0.0f, 0.0f, 0.0f, 1.0f };
	bool bFadeAudio = false;
};

// UE UMovieSceneFadeTrack (전역 트랙)
UCLASS()
class UMovieSceneFadeTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieSceneFadeTrack() = default;

	const char* GetDisplayName() const override { return "Fade"; }
	UMovieSceneSection* CreateNewSection() override;
};

// [OWN] UE에는 없는 트랙. C파트 CameraOverlay 패스의 Letterbox, Vignette를 움직인다
UCLASS()
class UMovieSceneCameraOverlaySection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneCameraOverlaySection() = default;

	int32 GetNumChannels() const override { return 2; }
	FMovieSceneFloatChannel* GetChannel(int32 Index) override { return Index == 0 ? &Letterbox : (Index == 1 ? &Vignette : nullptr); }
	const char* GetChannelName(int32 Index) const override { return Index == 0 ? "Letterbox" : "Vignette"; }
	void Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject) override;
	void SaveJson(nlohmann::json& Out) const override;
	void LoadJson(const nlohmann::json& In) override;

	FMovieSceneFloatChannel Letterbox;
	FMovieSceneFloatChannel Vignette;
	float LetterboxAspect = 2.39f;
};

UCLASS()
class UMovieSceneCameraOverlayTrack : public UMovieSceneTrack
{
	GENERATED_BODY()

public:
	UMovieSceneCameraOverlayTrack() = default;

	const char* GetDisplayName() const override { return "Camera Overlay"; }
	UMovieSceneSection* CreateNewSection() override;
};
