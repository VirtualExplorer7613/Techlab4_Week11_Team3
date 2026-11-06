#include "EnginePCH.h"
#include "MovieSceneTracks/MovieSceneSlomoTrack.h"

#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

UMovieSceneSlomoSection::UMovieSceneSlomoSection()
{
	FloatCurve.SetDefault(1.0f);
}

void UMovieSceneSlomoSection::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	UWorld* World = Player.GetPlaybackContext();
	if (!World) return;

	float Dilation = 1.0f;
	if (!FloatCurve.Evaluate(Context.GetTime(), Dilation)) return;

	if (ShouldRestoreState(Player))
	{
		Player.GetPreAnimatedState().SavePreAnimatedState(World, MovieScenePreAnimatedType::Slomo, [World]()
		{
			const float Original = World->GetCinematicTimeDilation();
			return [World, Original]() { World->SetCinematicTimeDilation(Original); };
		});
	}
	World->SetCinematicTimeDilation(Dilation);
}

UMovieSceneSection* UMovieSceneSlomoTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSlomoSection>(this);
}
