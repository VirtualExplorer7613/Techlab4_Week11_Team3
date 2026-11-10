#include "EnginePCH.h"
#include "AnimSequence.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "Containers/Map.h"
#include "Math/EngineMath.h"
#include "Render/SkeletalMeshData.h"

#include <cmath>

namespace
{
	FQuat QuatFromRotationRows(const FVector& X, const FVector& Y, const FVector& Z)
	{
		const float Trace = X.X + Y.Y + Z.Z;

		if (Trace > 0.0f)
		{
			const float FourW = 2.0f * std::sqrt(1.0f + Trace);
			return FQuat((Y.Z - Z.Y) / FourW, (Z.X - X.Z) / FourW, (X.Y - Y.X) / FourW, 0.25f * FourW);
		}

		if (X.X >= Y.Y && X.X >= Z.Z)
		{
			const float FourX = 2.0f * std::sqrt(1.0f + X.X - Y.Y - Z.Z);
			return FQuat(0.25f * FourX, (X.Y + Y.X) / FourX, (X.Z + Z.X) / FourX, (Y.Z - Z.Y) / FourX);
		}

		if (Y.Y >= Z.Z)
		{
			const float FourY = 2.0f * std::sqrt(1.0f + Y.Y - X.X - Z.Z);
			return FQuat((X.Y + Y.X) / FourY, 0.25f * FourY, (Y.Z + Z.Y) / FourY, (Z.X - X.Z) / FourY);
		}

		const float FourZ = 2.0f * std::sqrt(1.0f + Z.Z - X.X - Y.Y);
		return FQuat((X.Z + Z.X) / FourZ, (Y.Z + Z.Y) / FourZ, 0.25f * FourZ, (X.Y - Y.X) / FourZ);
	}

	FLocalBoneTransform SampleLocalBoneTransform(const FRawAnimSequenceTrack& Keys,	int32 FirstKey,	int32 SecondKey, float KeyAlpha)
	{
		FLocalBoneTransform Result{ Keys.PosKeys[FirstKey],	Keys.RotKeys[FirstKey],	Keys.ScaleKeys[FirstKey]  };

		if (KeyAlpha > 0.0f)
		{
			Result.Position = FMath::Lerp(Result.Position, Keys.PosKeys[SecondKey], KeyAlpha);
			Result.Rotation = FMath::Slerp(Result.Rotation, Keys.RotKeys[SecondKey], KeyAlpha);
			Result.Scale = FMath::Lerp(Result.Scale, Keys.ScaleKeys[SecondKey], KeyAlpha);
		}

		return Result;
	}
}

bool FLocalBoneTransform::TryFromMatrix(const FMatrix& Local, FLocalBoneTransform& OutTransform)
{
	const FVector Position(Local.M[3][0], Local.M[3][1], Local.M[3][2]);

	FVector X(Local.M[0][0], Local.M[0][1], Local.M[0][2]);
	FVector Y(Local.M[1][0], Local.M[1][1], Local.M[1][2]);
	FVector Z(Local.M[2][0], Local.M[2][1], Local.M[2][2]);

	FVector Scale(X.Size(), Y.Size(), Z.Size());

	const auto IsFinite = [](const FVector& V)
	{
		return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z);
	};

	constexpr float MinScale = 1e-6f;
	if (!IsFinite(Position) || !IsFinite(Scale) || Scale.X <= MinScale || Scale.Y <= MinScale || Scale.Z <= MinScale)
	{
		return false;
	}

	X = X / Scale.X;
	Y = Y / Scale.Y;
	Z = Z / Scale.Z;

	// 세 축이 서로 직교해야 위치·회전·크기로 표현할 수 있다.
	constexpr float OrthoTolerance = 1e-3f;
	if (std::abs(X.Dot(Y)) > OrthoTolerance || std::abs(X.Dot(Z)) > OrthoTolerance || std::abs(Y.Dot(Z)) > OrthoTolerance)
	{
		return false;
	}

	// 반사가 있는 경우 부호를 X 스케일에 저장합니다.
	if (X.Cross(Y).Dot(Z) < 0.0f)
	{
		X = X * -1.0f;
		Scale.X = -Scale.X;
	}

	OutTransform = { Position, QuatFromRotationRows(X, Y, Z).Normalized(), Scale };
	return true;
}

double UAnimSequence::GetSamplingDuration() const
{
	const UAnimDataModel* Model = GetDataModel();
	const int32 NumKeys = Model ? Model->GetNumberOfKeys() : 0;
	if (NumKeys <= 1)
	{
		return 0.0;
	}

	const FFrameRate FrameRate = Model->GetFrameRate();
	return FrameRate.Numerator > 0 && FrameRate.Denominator > 0 ? (NumKeys - 1) / FrameRate.AsDecimal() : 0.0;
}

bool UAnimSequence::EvaluatePoseAtTime(double TimeSeconds, const TArray<FBoneInfo>& Bones, TArray<FMatrix>& OutLocalPose) const
{
	TArray<std::optional<FLocalBoneTransform>> Samples;
	if (!SampleLocalPoseAtTime(TimeSeconds, Bones, Samples))
	{
		OutLocalPose.Reset();
		return false;
	}

	return BuildLocalPoseMatrices(Bones, Samples, OutLocalPose);
}

bool UAnimSequence::BuildLocalPoseMatrices(const TArray<FBoneInfo>& Bones, const TArray<std::optional<FLocalBoneTransform>>& Samples, TArray<FMatrix>& OutLocalPose)
{
	OutLocalPose.Reset();
	if (Bones.IsEmpty() || Samples.Num() != Bones.Num())
	{
		return false;
	}

	OutLocalPose.Reserve(Bones.Num());
	for (int32 BoneIndex = 0; BoneIndex < Bones.Num(); ++BoneIndex)
	{
		if (!Samples[BoneIndex])
		{
			OutLocalPose.Add(Bones[BoneIndex].RefLocal);
			continue;
		}

		const FLocalBoneTransform& Sample = *Samples[BoneIndex];
		FMatrix Local = Sample.Rotation.ToFMatrix();

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Local[0][Axis] *= Sample.Scale.X;
			Local[1][Axis] *= Sample.Scale.Y;
			Local[2][Axis] *= Sample.Scale.Z;
		}
		Local.SetOrigin(Sample.Position);
		OutLocalPose.Add(Local);
	}
	return true;
}

bool UAnimSequence::SampleLocalPoseAtTime(double TimeSeconds, const TArray<FBoneInfo>& Bones, TArray<std::optional<FLocalBoneTransform>>& OutSamples) const
{
	OutSamples.Reset();
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

	OutSamples.Reserve(Bones.Num());
	for (const FBoneInfo& Bone : Bones)
	{
		const FRawAnimSequenceTrack* Keys = TracksByBone.FindRef(FName(Bone.Name));
		if (!Keys)
		{
			OutSamples.Add(std::nullopt);
			continue;
		}

		OutSamples.Add(SampleLocalBoneTransform(*Keys, FirstKey, SecondKey, Alpha));
	}
	return true;
}
