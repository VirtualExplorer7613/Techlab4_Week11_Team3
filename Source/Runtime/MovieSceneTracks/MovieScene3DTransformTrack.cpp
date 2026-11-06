#include "EnginePCH.h"
#include "MovieSceneTracks/MovieScene3DTransformTrack.h"
#include "MovieSceneTracks/MovieScenePropertyBinding.h"

#include "Components/SceneComponent.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// 회전 키를 찍을 때 기존 커브 값에서 ±180° 안으로 맞춘다 (350° → 10°가 거꾸로 340° 돌지 않게)
	float UnwindDegrees(float Reference, float Value)
	{
		while (Value - Reference > 180.0f)  Value -= 360.0f;
		while (Value - Reference < -180.0f) Value += 360.0f;
		return Value;
	}
}

FMovieSceneFloatChannel* UMovieScene3DTransformSection::GetChannel(int32 Index)
{
	if (Index < 0 || Index >= 9) return nullptr;
	if (Index < 3) return &Translation[Index];
	if (Index < 6) return &Rotation[Index - 3];
	return &Scale[Index - 6];
}

const char* UMovieScene3DTransformSection::GetChannelName(int32 Index) const
{
	static const char* Names[9] = {
		"Location.X", "Location.Y", "Location.Z",
		"Rotation.Pitch", "Rotation.Yaw", "Rotation.Roll",
		"Scale.X", "Scale.Y", "Scale.Z" };
	return (Index >= 0 && Index < 9) ? Names[Index] : "";
}

void UMovieScene3DTransformSection::Evaluate(const FMovieSceneContext& Context, IMovieScenePlayer& Player, UObject* BoundObject)
{
	USceneComponent* Component = MovieSceneHelpers::SceneComponentFromBoundObject(BoundObject);
	if (!Component) return;

	if (ShouldRestoreState(Player))
	{
		Player.GetPreAnimatedState().SavePreAnimatedState(Component, MovieScenePreAnimatedType::Transform, [Component]()
		{
			const FVector Location = Component->GetWorldLocation();
			const FRotator Rotation = Component->GetWorldRotation();
			const FVector Scale3D = Component->GetWorldScale3D();
			return [Component, Location, Rotation, Scale3D]()
			{
				Component->SetWorldLocationAndRotation(Location, Rotation, false);
				Component->SetWorldScale3D(Scale3D);
			};
		});
	}

	// 키가 없는 채널은 현재 값을 그대로 둔다
	FVector Location = Component->GetWorldLocation();
	FRotator Rotation = Component->GetWorldRotation();
	FVector Scale3D = Component->GetWorldScale3D();
	float* Targets[9] = {
		&Location.X, &Location.Y, &Location.Z,
		&Rotation.Pitch, &Rotation.Yaw, &Rotation.Roll,
		&Scale3D.X, &Scale3D.Y, &Scale3D.Z };

	const FFrameTime Time = Context.GetTime();
	bool bLocationOrRotation = false;
	bool bScale = false;
	for (int32 Index = 0; Index < 9; ++Index)
	{
		if (!GetChannel(Index)->Evaluate(Time, *Targets[Index])) continue;
		(Index < 6 ? bLocationOrRotation : bScale) = true;
	}
	if (bLocationOrRotation) Component->SetWorldLocationAndRotation(Location, Rotation, false);
	if (bScale) Component->SetWorldScale3D(Scale3D);
}

void UMovieScene3DTransformSection::KeyCurrentValues(FFrameNumber Time, IMovieScenePlayer& Player, UObject* BoundObject)
{
	USceneComponent* Component = MovieSceneHelpers::SceneComponentFromBoundObject(BoundObject);
	if (!Component) return;

	const FVector Location = Component->GetWorldLocation();
	const FRotator Rotation = Component->GetWorldRotation();
	const FVector Scale3D = Component->GetWorldScale3D();
	const float Values[9] = {
		Location.X, Location.Y, Location.Z,
		Rotation.Pitch, Rotation.Yaw, Rotation.Roll,
		Scale3D.X, Scale3D.Y, Scale3D.Z };

	for (int32 Index = 0; Index < 9; ++Index)
	{
		FMovieSceneFloatChannel* Channel = GetChannel(Index);
		float Value = Values[Index];
		float Reference = 0.0f;
		if (Index >= 3 && Index < 6 && Channel->Evaluate(FFrameTime(Time), Reference))
			Value = UnwindDegrees(Reference, Value);
		Channel->UpdateOrAddKey(Time, Value);
	}
}

UMovieSceneSection* UMovieScene3DTransformTrack::CreateNewSection()
{
	// UE와 같이 새 Transform 섹션은 무한 구간
	return NewObject<UMovieScene3DTransformSection>(this);
}
