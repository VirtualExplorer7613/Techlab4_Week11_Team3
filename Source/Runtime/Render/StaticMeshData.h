#pragma once

#include "Vertex.h"
#include "Math/BVH.h"
#include "Math/Box.h"

// Cooked obj Data의 Section 구조체
struct FStaticMeshSection
{
	uint32 StartIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialSlotIndex = 0;
};

struct FStaticMaterialSlot
{
	FString Name;
	FVector4 BaseColor = FVector4(1, 1, 1, 1);
	FString DiffuseTexturePath;
	FString NormalTexturePath{};
};

struct FLODSourceVertex
{
	int32 PositionIndex = -1;
	int32 UVIndex = -1;
	int32 NormalIndex = -1;
};

struct FMeshTriangleElement
{
	FVector V0;
	FVector Edge1; // V1 - V0
	FVector Edge2; // V2 - V0
	FBox Bounds;
};

struct FStaticMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMaterialSlot> MaterialSlots;
	FBox AABB;

	// LOD0 생성용 CPU 메타데이터. GPU vertex layout에는 포함하지 않는다.
	TArray<FLODSourceVertex> LODSourceVertices;

	std::optional<TBVH<FMeshTriangleElement>> TriangleBVH;

	// Tangent가 이미 계산·임포트되어 있으면 true. CreateStaticMesh가 다시 계산하지 않는다.
	bool bTangentsBuilt = false;

	// TODO: 나중에 Sections, MaterialSlots 도 Append 해줘야 함.
	void Append(const FStaticMeshData& Other)
	{
		uint32 Base = (uint32)Vertices.Num();

		Vertices.Append(Other.Vertices);

		for (uint32 i : Other.Indices)
			Indices.Add(Base + i);
	}

	void Translate(const FVector& Offset)
	{
		for (auto& V : Vertices)
			V.Position += Offset;
	}

	// Vertices, Indices, Sections, MaterialSlots가 서로 맞는지 검사한다.
	// 실패하면 이유를 OutError에 담는다. 로그는 호출한 쪽이 경로와 함께 남긴다.
	bool Validate(FString& OutError) const;

	// 각 material section 안의 삼각형을 vertex 재사용이 가까워지도록 재배열한다.
	void OptimizeTriangleOrderForVertexCache();

	void BuildTriangleBVH();

	// MikkTSpace로 Tangent를 계산한다. bKeepExisting이면 Tangent.W != 0인 정점(임포트한 값)은 그대로 둔다.
	// 한 정점을 쓰는 코너끼리 Tangent가 다르면 정점을 복제해 나누므로 Vertices·Indices·LODSourceVertices가 늘 수 있다.
	void BuildTangentSpace(bool bKeepExisting = false);
};
