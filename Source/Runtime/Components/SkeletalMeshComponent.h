#pragma once

#include "SkinnedMeshComponent.h"
#include "SkeletalMeshComponent.generated.h"

class UAnimSequence;

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()
public:
	USkeletalMeshComponent();

	// 1. Local 자세 편집
	void ResetToRefPose();
	bool SetBoneLocalRotation(const FString& BoneName, const FRotator& Rotation);
	// 인덱스로 뼈 하나의 Local(부모 기준) 행렬을 바꾼다. 반영은 ApplyPose
	bool SetBoneLocalTransform(int32 BoneIndex, const FMatrix& Local);
	void ApplyPose() { RefreshSkinning(LocalPose); }
	bool ApplyLocalPose(TArray<FMatrix> InLocalPose);
	bool ApplyAnimationPose(const UAnimSequence& Sequence, double TimeSeconds);
	bool PlayAnimation(const UAnimSequence* Sequence, bool bLooping = false);
	void TickComponent(float DeltaTime) override;

	virtual void PostEditChangeProperty(const FProperty& Property) override;

protected:
	virtual void OnSkeletalMeshChanged() override;

	// 뼈별 Local 자세 (부모 기준)
	TArray<FMatrix> LocalPose;

	// 테스트용: Details에서 본 하나 돌려 보기 (Step 6 완료 기준)
	UPROPERTY(EditAnywhere)
	FString TestBoneName = "upperarm_l";
	UPROPERTY(EditAnywhere)
	FRotator TestBoneRotation;

private:
	const UAnimSequence* PlayingSequence = nullptr;
	double PlaybackTime = 0.0;
	bool bLoopAnimation = false;
};
