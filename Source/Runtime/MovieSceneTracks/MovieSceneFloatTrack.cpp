#include "EnginePCH.h"
#include "MovieSceneTracks/MovieSceneFloatTrack.h"

#include "UObject/UObjectGlobals.h"
#include <functional>

namespace
{
	const UMovieScenePropertyTrack* OwningPropertyTrack(const UMovieSceneSection* Section)
	{
		return Cast<UMovieScenePropertyTrack>(Section->GetOuter());
	}

	uint32 PropertyStateID(const FString& PropertyName)
	{
		return MovieScenePreAnimatedType::FloatProperty ^ static_cast<uint32>(std::hash<FString>{}(PropertyName) << 4);
	}
}

const char* UMovieSceneFloatSection::GetChannelName(int32 Index) const
{
	const UMovieScenePropertyTrack* Track = OwningPropertyTrack(this);
	return Track ? Track->GetPropertyBinding().PropertyName.c_str() : "Value";
}

void UMovieSceneFloatSection::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	const UMovieScenePropertyTrack* Track = OwningPropertyTrack(this);
	if (!Track || !BoundObject) return;
	const FString& PropertyName = Track->GetPropertyBinding().PropertyName;

	float Value = 0.0f;
	if (!FloatCurve.Evaluate(Context.GetTime(), Value)) return;

	if (ShouldRestoreState(Player))
	{
		Player.GetPreAnimatedState().SavePreAnimatedState(BoundObject, PropertyStateID(PropertyName), [BoundObject, PropertyName]()
		{
			float Original = 0.0f;
			const bool bHasOriginal = MovieScenePropertyBindings::GetFloat(BoundObject, PropertyName, Original);
			return [BoundObject, PropertyName, Original, bHasOriginal]()
			{
				if (bHasOriginal) MovieScenePropertyBindings::SetFloat(BoundObject, PropertyName, Original);
			};
		});
	}
	MovieScenePropertyBindings::SetFloat(BoundObject, PropertyName, Value);
}

void UMovieSceneFloatSection::KeyCurrentValues(FFrameNumber Time, IMovieScenePlayer& Player, UObject* BoundObject)
{
	const UMovieScenePropertyTrack* Track = OwningPropertyTrack(this);
	float Value = 0.0f;
	if (Track && MovieScenePropertyBindings::GetFloat(BoundObject, Track->GetPropertyBinding().PropertyName, Value))
		FloatCurve.UpdateOrAddKey(Time, Value);
}

void UMovieScenePropertyTrack::SaveJson(nlohmann::json& Out) const
{
	Super::SaveJson(Out);
	Out["PropertyName"] = PropertyBinding.PropertyName;
}

void UMovieScenePropertyTrack::LoadJson(const nlohmann::json& In)
{
	PropertyBinding.PropertyName = In.value("PropertyName", FString());
	Super::LoadJson(In);
}

UMovieSceneSection* UMovieSceneFloatTrack::CreateNewSection()
{
	return NewObject<UMovieSceneFloatSection>(this);
}
