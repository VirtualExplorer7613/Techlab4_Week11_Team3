#include "EnginePCH.h"
#include "AnimDataController.h"
#include "Core/EngineLog.h"

void UAnimDataController::SetModel(UAnimDataModel* InModel)
{
	Model = InModel;
}

void UAnimDataController::SetFrameRate(FFrameRate FrameRate)
{
	if (!Model)
	{
		HTR_LOG(Error, "Invalid Model!");
		return;
	}

	if (FrameRate.Numerator <= 0 || FrameRate.Denominator <= 0)
	{
		HTR_LOG(Error, "Invalid FrameRate: {}/{}", FrameRate.Numerator, FrameRate.Denominator);
		return;
	}

	Model->FrameRate = FrameRate;
	const double FPS = Model->FrameRate.AsDecimal();
	Model->PlayLength = static_cast<float>(Model->NumberOfFrames.Value / FPS);

}

bool UAnimDataController::SetNumberOfFrames(FFrameNumber InNumberOfFrames)
{
	if (!Model)
	{
		HTR_LOG(Error, "Invalid Model!");
		return false;
	}

	if (InNumberOfFrames.Value < 0)
	{
		HTR_LOG(Error, "Frame count must not be negative: {}", InNumberOfFrames.Value);
		return false;
	}

	const FFrameNumber CurrentNumberOfFrames = Model->GetNumberOfFrames();
	
	const int32 DeltaFrames = std::abs(InNumberOfFrames.Value - CurrentNumberOfFrames.Value);

	const FFrameNumber T0 = InNumberOfFrames > CurrentNumberOfFrames ? CurrentNumberOfFrames : CurrentNumberOfFrames - DeltaFrames;
	const FFrameNumber T1 = InNumberOfFrames > CurrentNumberOfFrames ? InNumberOfFrames : CurrentNumberOfFrames;

	return ResizeNumberOfFrames(InNumberOfFrames, T0, T1);
}

bool UAnimDataController::ResizeNumberOfFrames(FFrameNumber NewLength, FFrameNumber T0, FFrameNumber T1)
{
	if (!Model || NewLength.Value < 0)
	{
		HTR_LOG(Error, "Invalid model or frame count.");
		return false;
	}

	// 같은 길이면 변경할 필요가 없다.
	if (NewLength == Model->GetNumberOfFrames())
	{
		return true;
	}

	const FFrameRate Rate = Model->GetFrameRate();

	if (Rate.Numerator <= 0 || Rate.Denominator <= 0)
	{
		HTR_LOG(Error, "Invalid FrameRate.");
		return false;
	}


	// 길이가 달라지는 경우에만 변경 구간을 검사
	if (NewLength != Model->GetNumberOfFrames())
	{
		const int32 PlayRangeEnd = FMath::Max(1, Model->GetNumberOfKeys());

		if (T0.Value < 0 || T0.Value >= PlayRangeEnd || T0 >= T1)
		{
			HTR_LOG(Error, "Invalid resize range: {} to {}", T0.Value, T1.Value);
			return false;
		}
	}

	// 모든 검증이 끝난 뒤 함께 갱신
	Model->NumberOfFrames = NewLength;
	Model->NumberOfKeys = NewLength.Value + 1;
	Model->PlayLength = static_cast<float>(NewLength.Value / Rate.AsDecimal());

	return true;
}


bool UAnimDataController::AddBoneTrack(const FName& BoneName)
{
	// SetModel()로 대상을 지정하지 않았으면 중단
	if (!Model || BoneName.IsNone())
	{
		return false;
	}

	for (const FBoneAnimationTrack& Track : Model->BoneAnimationTracks)
	{
		if (BoneName == Track.Name)
		{
			return false;
		}
	}

	// 새로운 Bone track 만들고 이름 부여
	FBoneAnimationTrack NewTrack;
	NewTrack.Name = BoneName;
	Model->BoneAnimationTracks.Add(NewTrack);

	return true;
}

bool UAnimDataController::SetBoneTrackKeys(const FName& BoneName, const TArray<FVector>& PosKeys, const TArray<FQuat>& RotKeys, const TArray<FVector>& ScaleKeys)
{
	if (!Model || BoneName.IsNone())
	{
		HTR_LOG(Error, "Invalid model or BoneName.");
		return false;
	}

	const int32 ExpectedNumKeys = Model->GetNumberOfKeys();

	const bool bValidPosKeys = PosKeys.Num() == ExpectedNumKeys;
	const bool bValidRotKeys = RotKeys.Num() == ExpectedNumKeys;
	const bool bValidScaleKeys = ScaleKeys.Num() == ExpectedNumKeys;

	// 하나라도 조건을 만족하지 않으면 저장하지 않는다.
	if (ExpectedNumKeys <= 0 || !bValidPosKeys || !bValidRotKeys || !bValidScaleKeys)
	{
		return false;
	}

	for (FBoneAnimationTrack& Track : Model->BoneAnimationTracks)
	{
		if (Track.Name == BoneName)
		{
			Track.InternalTrack.PosKeys = PosKeys;
			Track.InternalTrack.RotKeys = RotKeys;
			Track.InternalTrack.ScaleKeys = ScaleKeys;

			return true;
		}
	}
	
	// 끝까지 검색했는데 해당 본의 트랙이 없는 경우
	return false;
}
