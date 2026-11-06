#include "EnginePCH.h"
#include "MovieSceneTracks/MovieSceneFadeTrack.h"

#include "Render/SceneView.h"
#include "UObject/UObjectGlobals.h"

// UE 페이드 시스템은 PlayerCameraManager->SetManualCameraFade를 부른다.
// 이 엔진은 PCM이 아직 CameraOverlay와 연결되지 않아 GCameraOverlayDebug에 직접 쓴다.
namespace
{
	void SaveOverlayState(IMovieScenePlayer& Player, uint32 TypeID)
	{
		Player.GetPreAnimatedState().SavePreAnimatedState(&GCameraOverlayDebug, TypeID, []()
		{
			const FCameraOverlayDebug Original = GCameraOverlayDebug;
			return [Original]() { GCameraOverlayDebug = Original; };
		});
	}
}

void UMovieSceneFadeSection::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	float FadeValue = 0.0f;
	if (!FloatCurve.Evaluate(Context.GetTime(), FadeValue)) return;
	if (ShouldRestoreState(Player)) SaveOverlayState(Player, MovieScenePreAnimatedType::Fade);

	GCameraOverlayDebug.bOverride = true;
	GCameraOverlayDebug.FadeAmount = std::clamp(FadeValue, 0.0f, 1.0f);
	GCameraOverlayDebug.FadeColor[0] = FadeColor.R;
	GCameraOverlayDebug.FadeColor[1] = FadeColor.G;
	GCameraOverlayDebug.FadeColor[2] = FadeColor.B;
}

void UMovieSceneFadeSection::SaveJson(nlohmann::json& Out) const
{
	Super::SaveJson(Out);
	Out["FadeColor"] = { FadeColor.R, FadeColor.G, FadeColor.B, FadeColor.A };
	Out["FadeAudio"] = bFadeAudio;
}

void UMovieSceneFadeSection::LoadJson(const nlohmann::json& In)
{
	Super::LoadJson(In);
	if (const auto It = In.find("FadeColor"); It != In.end() && It->is_array() && It->size() == 4)
		FadeColor = FLinearColor{ (*It)[0].get<float>(), (*It)[1].get<float>(), (*It)[2].get<float>(), (*It)[3].get<float>() };
	bFadeAudio = In.value("FadeAudio", false);
}

UMovieSceneSection* UMovieSceneFadeTrack::CreateNewSection()
{
	return NewObject<UMovieSceneFadeSection>(this);
}

void UMovieSceneCameraOverlaySection::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	float LetterboxValue = 0.0f;
	float VignetteValue = 0.0f;
	const bool bLetterbox = Letterbox.Evaluate(Context.GetTime(), LetterboxValue);
	const bool bVignette = Vignette.Evaluate(Context.GetTime(), VignetteValue);
	if (!bLetterbox && !bVignette) return;
	if (ShouldRestoreState(Player)) SaveOverlayState(Player, MovieScenePreAnimatedType::CameraOverlay);

	GCameraOverlayDebug.bOverride = true;
	if (bLetterbox)
	{
		GCameraOverlayDebug.LetterboxAmount = std::clamp(LetterboxValue, 0.0f, 1.0f);
		GCameraOverlayDebug.LetterboxAspect = LetterboxAspect;
	}
	if (bVignette) GCameraOverlayDebug.VignetteIntensity = std::max(VignetteValue, 0.0f);
}

void UMovieSceneCameraOverlaySection::SaveJson(nlohmann::json& Out) const
{
	Super::SaveJson(Out);
	Out["LetterboxAspect"] = LetterboxAspect;
}

void UMovieSceneCameraOverlaySection::LoadJson(const nlohmann::json& In)
{
	Super::LoadJson(In);
	LetterboxAspect = In.value("LetterboxAspect", 2.39f);
}

UMovieSceneSection* UMovieSceneCameraOverlayTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraOverlaySection>(this);
}
