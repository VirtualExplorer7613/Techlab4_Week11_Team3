#include "EnginePCH.h"
#include "SkeletalMeshObject.h"
#include "Render/RenderCommand.h"
#include "Render/SkeletalMesh.h"
#include <algorithm>

FSkeletalMeshObjectCPUSkin::FSkeletalMeshObjectCPUSkin(const USkeletalMesh* InSkeletalMesh)
	: SkeletalMesh(InSkeletalMesh)
{
	if (!SkeletalMesh || SkeletalMesh->Data.Mesh.Vertices.IsEmpty())
		return;

	// 처음에는 바인드 자세 그대로 그린다
	const FStaticMeshData& BindPose = SkeletalMesh->Data.Mesh;
	SkinnedVertices = BindPose.Vertices;
	LocalBounds = BindPose.AABB;

	// 매 갱신마다 통째로 덮어쓰므로 Dynamic
	const uint32 VertexBytes = sizeof(FVertex) * static_cast<uint32>(SkinnedVertices.Num());
	VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(VertexBytes, sizeof(FVertex));
	if (VertexBuffer)
		Upload();
}

void FSkeletalMeshObjectCPUSkin::Update(const TArray<FMatrix>& SkinMatrices)
{
	if (!IsValid())
		return;

	const TArray<FSkinWeight>& Weights = SkeletalMesh->Data.SkinWeights;
	const TArray<FVertex>& Source = SkeletalMesh->Data.Mesh.Vertices;
	if (Weights.Num() != Source.Num() || SkinnedVertices.Num() != Source.Num())
	{
		HTR_LOG(Warning, "Skinning skipped: weights {}, verts {}", Weights.Num(), Source.Num());
		return;
	}

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// 정점마다 뼈 4개의 결과를 비율대로 섞기
	for (int32 v = 0; v < Source.Num(); ++v)
	{
		const FVertex& In = Source[v];
		const FVector InTangent(In.Tangent.X, In.Tangent.Y, In.Tangent.Z);
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		FVector Tangent = FVector::ZeroVector;

		for (int32 k = 0; k < 4; ++k)
		{
			const float W = Weights[v].Weights[k];
			const int32 Bone = Weights[v].BoneIndices[k];
			if (W <= 0.0f || Bone < 0 || Bone >= SkinMatrices.Num())
				continue;

			const FMatrix& S = SkinMatrices[Bone];
			Position += S.TransformPosition(In.Position) * W;
			Normal += S.TransformVector(In.Normal) * W;
			Tangent += S.TransformVector(InTangent) * W;
		}

		FVertex& Out = SkinnedVertices[v];
		Out.Position = Position;
		Out.Normal = Normal.GetSafeNormal(SMALL_NUMBER, In.Normal);
		const FVector T = Tangent.GetSafeNormal(SMALL_NUMBER, InTangent);
		Out.Tangent = FVector4(T.X, T.Y, T.Z, In.Tangent.W);

		Min = FVector(std::min(Min.X, Position.X), std::min(Min.Y, Position.Y), std::min(Min.Z, Position.Z));
		Max = FVector(std::max(Max.X, Position.X), std::max(Max.Y, Position.Y), std::max(Max.Z, Position.Z));
	}

	LocalBounds = FBox{ Min, Max };
	Upload();
}

void FSkeletalMeshObjectCPUSkin::Upload()
{
	// Map WRITE_DISCARD로 통째로 덮어쓴다
	RenderCommand::UpdateBufferData(VertexBuffer.get(), SkinnedVertices.GetData(),
		sizeof(FVertex) * static_cast<uint32>(SkinnedVertices.Num()));
}
