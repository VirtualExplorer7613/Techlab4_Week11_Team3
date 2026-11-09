#include "EnginePCH.h"
#include "AnimSequence.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Containers/Map.h"
#include "Math/EngineMath.h"
#include "Render/SkeletalMeshData.h"

#include <cmath>

bool UAnimSequence::EvaluatePoseAtTime(double TimeSeconds, const TArray<FBoneInfo>& Bones, TArray<FMatrix>& OutLocalPose) const
{
	OutLocalPose.Reset();
	const UAnimDataModel* Model = GetDataModel();
	if (!Model || Bones.IsEmpty() || !std::isfinite(TimeSeconds))
	{
		return false;
	}

	const int32 NumKeys = Model->GetNumberOfKeys();
	const FFrameRate FrameRate = Model->GetFrameRate();
	if (NumKeys <= 0 || FrameRate.Numerator <= 0 || FrameRate.Denominator <= 0)
	{
		return false;
	}

	const int32 LastKey = NumKeys - 1;
	const double KeyPosition = FMath::Clamp(TimeSeconds * FrameRate.AsDecimal(), 0.0, static_cast<double>(LastKey));
	const int32 FirstKey = static_cast<int32>(KeyPosition);
	const int32 SecondKey = FMath::Min(FirstKey + 1, LastKey);
	const float Alpha = static_cast<float>(KeyPosition - FirstKey);

	const TArray<FBoneAnimationTrack>& Tracks = Model->GetBoneAnimationTracks();
	TMap<FName, const FRawAnimSequenceTrack*, FNameHash> TracksByBone;
	TracksByBone.Reserve(Tracks.Num());
	for (const FBoneAnimationTrack& Track : Tracks)
	{
		const FRawAnimSequenceTrack& Keys = Track.InternalTrack;
		if (Keys.PosKeys.Num() == NumKeys && Keys.RotKeys.Num() == NumKeys && Keys.ScaleKeys.Num() == NumKeys)
		{
			TracksByBone.Add(Track.Name, &Keys);
		}
	}

	OutLocalPose.Reserve(Bones.Num());
	for (const FBoneInfo& Bone : Bones)
	{
		const FRawAnimSequenceTrack* Keys = TracksByBone.FindRef(FName(Bone.Name));
		if (!Keys)
		{
			OutLocalPose.Add(Bone.RefLocal);
			continue;
		}

		FVector Position = Keys->PosKeys[FirstKey];
		FQuat Rotation = Keys->RotKeys[FirstKey];
		FVector Scale = Keys->ScaleKeys[FirstKey];
		if (Alpha > 0.0f)
		{
			Position = FMath::Lerp(Position, Keys->PosKeys[SecondKey], Alpha);
			Rotation = FMath::Slerp(Rotation, Keys->RotKeys[SecondKey], Alpha);
			Scale = FMath::Lerp(Scale, Keys->ScaleKeys[SecondKey], Alpha);
		}

		// 행벡터 기준으로 크기, 회전, 위치 순서로 적용한다.
		FMatrix Local = Rotation.ToFMatrix();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Local[0][Axis] *= Scale.X;
			Local[1][Axis] *= Scale.Y;
			Local[2][Axis] *= Scale.Z;
		}
		Local.SetOrigin(Position);
		OutLocalPose.Add(Local);
	}
	return true;
}
