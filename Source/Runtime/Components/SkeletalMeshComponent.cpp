#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"
#include "Asset/AssetManager.h"
#include "Core/EngineLog.h"
#include "Math/Quat.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
	// 배치 직후 보이도록 기본 에셋을 둔다 (StaticMeshComponent의 Cube와 같은 역할). Details에서 바꾼다
	SkeletalMesh = UAssetManager::GetAssetByPath<USkeletalMesh>("Models/SKM_UnrealMan/SKM_Manny_Simple.FBX");
	ResetToRefPose();
}

void USkeletalMeshComponent::OnSkeletalMeshChanged()
{
	// 뼈 수와 바인드 자세가 에셋마다 다르므로 이전 에셋의 자세는 버린다
	ResetToRefPose();
	Super::OnSkeletalMeshChanged();
}

void USkeletalMeshComponent::ResetToRefPose()
{
	LocalPose.Reset();
	if (!SkeletalMesh)
		return;

	for (const FBoneInfo& Bone : SkeletalMesh->Data.Bones)
	{
		LocalPose.Add(Bone.RefLocal);
	}
}

bool USkeletalMeshComponent::SetBoneLocalRotation(const FString& BoneName, const FRotator& Rotation)
{
	if (!SkeletalMesh)
		return false;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->Data.Bones;
	if (LocalPose.Num() != Bones.Num())
		ResetToRefPose();

	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		if (Bones[i].Name == BoneName)
		{
			// 뼈 자기 축으로 돌린 뒤 부모 기준 자리로 (행벡터: 왼쪽부터 적용)
			LocalPose[i] = Rotation.Quaternion().ToFMatrix() * Bones[i].RefLocal;
			return true;
		}
	}
	return false;
}

bool USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FMatrix& Local)
{
	if (!SkeletalMesh || !SkeletalMesh->Data.Bones.IsValidIndex(BoneIndex))
		return false;
	if (LocalPose.Num() != SkeletalMesh->Data.Bones.Num())
		ResetToRefPose();
	LocalPose[BoneIndex] = Local;
	return true;
}

void USkeletalMeshComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Property.Name == "TestBoneName" || Property.Name == "TestBoneRotation")
	{
		ResetToRefPose();
		if (!SetBoneLocalRotation(TestBoneName, TestBoneRotation))
		{
			HTR_LOG(Warning, "Bone not found: {}", TestBoneName);
		}
		ApplyPose();
	}
}
