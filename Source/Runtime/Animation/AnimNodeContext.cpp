#include "EnginePCH.h"
#include "AnimNodeContext.h"

#include "Math/EngineMath.h"
#include "Render/SkeletalMeshData.h"

void FPoseContext::BlendTwoPoses(const FPoseContext& PoseA, const FPoseContext& PoseB, float Alpha)
{
    Samples.Reset();
    const int32 BoneCount = Bones.Num();
    if (BoneCount == 0 || PoseA.Samples.Num() != BoneCount || PoseB.Samples.Num() != BoneCount)
    {
        return;
    }

    Samples.Reserve(BoneCount);

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const auto& SampleA = PoseA.Samples[BoneIndex];
        const auto& SampleB = PoseB.Samples[BoneIndex];

        if (!SampleA && !SampleB)
        {
            Samples.Add(std::nullopt);
            continue;
        }

        FLocalBoneTransform RefPose;
        if ((!SampleA || !SampleB) &&
            !FLocalBoneTransform::TryFromMatrix(Bones[BoneIndex].RefLocal, RefPose))
        {
            // 레퍼런스 행렬을 분해할 수 없으면 이 본에만 비중이 큰 쪽의 자세를 쓴다.
            Samples.Add(Alpha < 0.5f ? SampleA : SampleB);
            continue;
        }

        const FLocalBoneTransform& TransformA = SampleA ? *SampleA : RefPose;
        const FLocalBoneTransform& TransformB = SampleB ? *SampleB : RefPose;
        const FLocalBoneTransform Blended{
            FMath::Lerp(TransformA.Position, TransformB.Position, Alpha),
            FMath::Slerp(TransformA.Rotation, TransformB.Rotation, Alpha),
            FMath::Lerp(TransformA.Scale, TransformB.Scale, Alpha)
        };
        Samples.Add(Blended);
    }
}
