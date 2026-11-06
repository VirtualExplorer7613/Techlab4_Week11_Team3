#include "EnginePCH.h"
#include "StaticMeshData.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <format>
#include <vector>

#include "mikktspace.h"

namespace
{
	// MikkTSpace 콜백이 받는 사용자 데이터. 결과는 코너(삼각형 × 3)마다 따로 받는다.
	struct FMikkContext
	{
		const FStaticMeshData* Mesh = nullptr;
		TArray<FVector4> CornerTangents; // xyz = Tangent, w = Handedness
	};

	const FVertex& GetMikkVertex(const SMikkTSpaceContext* Context, int Face, int Corner)
	{
		const FStaticMeshData* Mesh = static_cast<const FMikkContext*>(Context->m_pUserData)->Mesh;
		return Mesh->Vertices[Mesh->Indices[Face * 3 + Corner]];
	}

	int MikkGetNumFaces(const SMikkTSpaceContext* Context)
	{
		return static_cast<const FMikkContext*>(Context->m_pUserData)->Mesh->Indices.Num() / 3;
	}

	int MikkGetNumVerticesOfFace(const SMikkTSpaceContext*, const int)
	{
		return 3;
	}

	void MikkGetPosition(const SMikkTSpaceContext* Context, float Out[], const int Face, const int Corner)
	{
		const FVector& P = GetMikkVertex(Context, Face, Corner).Position;
		Out[0] = P.X; Out[1] = P.Y; Out[2] = P.Z;
	}

	void MikkGetNormal(const SMikkTSpaceContext* Context, float Out[], const int Face, const int Corner)
	{
		const FVector& N = GetMikkVertex(Context, Face, Corner).Normal;
		Out[0] = N.X; Out[1] = N.Y; Out[2] = N.Z;
	}

	void MikkGetTexCoord(const SMikkTSpaceContext* Context, float Out[], const int Face, const int Corner)
	{
		const FVector2& UV = GetMikkVertex(Context, Face, Corner).UV;
		Out[0] = UV.X; Out[1] = UV.Y;
	}

	// Handedness는 MikkTSpace의 fSign 대신 실제 Bitangent로 직접 구한다.
	// fSign은 삼각형 감기 방향을 따르는데, 엔진은 FBX/OBJ의 감기를 뒤집어 쓰므로 부호가 엇갈릴 수 있다.
	void MikkSetTSpace(const SMikkTSpaceContext* Context, const float Tangent[], const float Bitangent[],
		const float, const float, const tbool, const int Face, const int Corner)
	{
		FMikkContext* Mikk = static_cast<FMikkContext*>(Context->m_pUserData);
		const FVector T(Tangent[0], Tangent[1], Tangent[2]);
		const FVector B(Bitangent[0], Bitangent[1], Bitangent[2]);
		const FVector& N = GetMikkVertex(Context, Face, Corner).Normal;
		const float Handedness = N.Cross(T).Dot(B) < 0.0f ? -1.0f : 1.0f;
		Mikk->CornerTangents[Face * 3 + Corner] = FVector4(T, Handedness);
	}

	bool IsSameTangent(const FVector4& A, const FVector4& B)
	{
		return A.W == B.W && FVector(A.X, A.Y, A.Z).Dot(FVector(B.X, B.Y, B.Z)) > 0.999f;
	}

	uint32 CountVertexCacheMisses(const uint32* Indices, uint32 IndexCount, uint32 CacheSize)
	{
		std::vector<uint32> Cache;
		Cache.reserve(CacheSize);
		uint32 Misses = 0;
		for (uint32 I = 0; I < IndexCount; ++I)
		{
			const uint32 Vertex = Indices[I];
			const auto It = std::find(Cache.begin(), Cache.end(), Vertex);
			if (It == Cache.end())
				++Misses;
			else
				Cache.erase(It);
			Cache.insert(Cache.begin(), Vertex);
			if (Cache.size() > CacheSize)
				Cache.pop_back();
		}
		return Misses;
	}
}

bool FStaticMeshData::Validate(FString& OutError) const
{
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	const uint32 IndexCount = static_cast<uint32>(Indices.Num());

	if (VertexCount == 0 || IndexCount == 0 || IndexCount % 3 != 0)
	{
		OutError = std::format("invalid mesh (vertices {}, indices {})", VertexCount, IndexCount);
		return false;
	}

	for (uint32 Index : Indices)
	{
		if (Index >= VertexCount)
		{
			OutError = std::format("index {} out of range (vertices {})", Index, VertexCount);
			return false;
		}
	}

	uint32 SectionIndexSum = 0;
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.StartIndex + Section.IndexCount > IndexCount)
		{
			OutError = std::format("section range {}+{} exceeds indices {}", Section.StartIndex, Section.IndexCount, IndexCount);
			return false;
		}
		if (Section.MaterialSlotIndex >= static_cast<uint32>(MaterialSlots.Num()))
		{
			OutError = std::format("material slot {} out of range (slots {})", Section.MaterialSlotIndex, MaterialSlots.Num());
			return false;
		}
		SectionIndexSum += Section.IndexCount;
	}
	if (SectionIndexSum != IndexCount)
	{
		OutError = std::format("section index sum {} != indices {}", SectionIndexSum, IndexCount);
		return false;
	}

	if (!LODSourceVertices.IsEmpty() &&
		LODSourceVertices.Num() != Vertices.Num())
	{
		OutError = "LOD source vertex count does not match vertex count";
		return false;
	}

	return true;
}

void FStaticMeshData::OptimizeTriangleOrderForVertexCache()
{
	constexpr uint32 CacheSize = 32;
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	for (const FStaticMeshSection& Section : Sections)
	{
		if (Section.IndexCount < 6 || Section.StartIndex % 3 != 0 ||
			Section.IndexCount % 3 != 0 ||
			Section.StartIndex > static_cast<uint32>(Indices.Num()) ||
			Section.IndexCount > static_cast<uint32>(Indices.Num()) - Section.StartIndex)
			continue;

		const uint32 TriangleCount = Section.IndexCount / 3;
		const uint32* Source = Indices.GetData() + Section.StartIndex;
		std::vector<std::vector<uint32>> Adjacent(VertexCount);
		std::vector<uint32> Remaining(VertexCount, 0);
		for (uint32 T = 0; T < TriangleCount; ++T)
		{
			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 V = Source[T * 3 + Corner];
				if (V >= VertexCount) return;
				Adjacent[V].push_back(T);
				++Remaining[V];
			}
		}

		std::vector<uint8> Emitted(TriangleCount, 0);
		std::vector<uint32> Seen(TriangleCount, 0);
		std::vector<int32> CachePosition(VertexCount, -1);
		std::vector<uint32> Cache;
		Cache.reserve(CacheSize);
		std::vector<uint32> Reordered;
		Reordered.reserve(Section.IndexCount);
		uint32 NextTriangle = 0;
		uint32 Pass = 0;

		while (Reordered.size() < Section.IndexCount)
		{
			++Pass;
			uint32 Best = TriangleCount;
			float BestScore = -FLT_MAX;
			for (uint32 CachedVertex : Cache)
			{
				for (uint32 T : Adjacent[CachedVertex])
				{
					if (Emitted[T] || Seen[T] == Pass) continue;
					Seen[T] = Pass;
					float Score = 0.0f;
					for (uint32 Corner = 0; Corner < 3; ++Corner)
					{
						const uint32 V = Source[T * 3 + Corner];
						const int32 Position = CachePosition[V];
						if (Position >= 0)
						{
							Score += Position < 3 ? 0.75f :
								std::pow(static_cast<float>(CacheSize - Position) /
									static_cast<float>(CacheSize - 3), 1.5f);
						}
						Score += 2.0f / std::sqrt(static_cast<float>(Remaining[V]));
					}
					if (Score > BestScore)
					{
						BestScore = Score;
						Best = T;
					}
				}
			}

			if (Best == TriangleCount)
			{
				while (NextTriangle < TriangleCount && Emitted[NextTriangle]) ++NextTriangle;
				Best = NextTriangle;
			}
			for (uint32 CachedVertex : Cache) CachePosition[CachedVertex] = -1;
			Emitted[Best] = 1;
			for (uint32 Corner = 0; Corner < 3; ++Corner)
			{
				const uint32 V = Source[Best * 3 + Corner];
				Reordered.push_back(V);
				--Remaining[V];
				const auto It = std::find(Cache.begin(), Cache.end(), V);
				if (It != Cache.end()) Cache.erase(It);
				Cache.insert(Cache.begin(), V);
				if (Cache.size() > CacheSize) Cache.pop_back();
			}
			for (uint32 I = 0; I < Cache.size(); ++I)
				CachePosition[Cache[I]] = static_cast<int32>(I);
		}

		const uint32 Old16 = CountVertexCacheMisses(Source, Section.IndexCount, 16);
		const uint32 Old32 = CountVertexCacheMisses(Source, Section.IndexCount, 32);
		const uint32 New16 = CountVertexCacheMisses(Reordered.data(), Section.IndexCount, 16);
		const uint32 New32 = CountVertexCacheMisses(Reordered.data(), Section.IndexCount, 32);
		if (New16 <= Old16 && New32 <= Old32 && (New16 < Old16 || New32 < Old32))
		{
			for (uint32 I = 0; I < Section.IndexCount; ++I)
				Indices[Section.StartIndex + I] = Reordered[I];
		}
	}
}

void FStaticMeshData::BuildTriangleBVH()
{
	TArray<FMeshTriangleElement> Elements;
	for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		const FVector& V0 = Vertices[Indices[i]].Position;
		const FVector& V1 = Vertices[Indices[i + 1]].Position;
		const FVector& V2 = Vertices[Indices[i + 2]].Position;

		FMeshTriangleElement Element = {
			.V0 = V0,
			.Edge1 = V1 - V0,
			.Edge2 = V2 - V0,
		};

		Element.Bounds.Min.X = std::min({ V0.X, V1.X, V2.X });
		Element.Bounds.Min.Y = std::min({ V0.Y, V1.Y, V2.Y });
		Element.Bounds.Min.Z = std::min({ V0.Z, V1.Z, V2.Z });
		Element.Bounds.Max.X = std::max({ V0.X, V1.X, V2.X });
		Element.Bounds.Max.Y = std::max({ V0.Y, V1.Y, V2.Y });
		Element.Bounds.Max.Z = std::max({ V0.Z, V1.Z, V2.Z });
		Elements.Add(Element);
	}

	TriangleBVH.emplace([](const FMeshTriangleElement& Element) {
		return Element.Bounds;
	});

	TriangleBVH->Build(Elements);
}

void FStaticMeshData::BuildTangentSpace(bool bKeepExisting) {
	const int32 IndexCount{ Indices.Num() };
	if (IndexCount < 3 || Vertices.IsEmpty()) {
		return;
	}

	// 1. MikkTSpace로 코너마다 Tangent 계산
	FMikkContext Mikk;
	Mikk.Mesh = this;
	Mikk.CornerTangents.SetNum(IndexCount);
	for (FVector4& Tangent : Mikk.CornerTangents) {
		Tangent = FVector4{ 1.0f, 0.0f, 0.0f, 1.0f }; // MikkTSpace가 건너뛴 코너의 기본값
	}

	SMikkTSpaceInterface Interface{};
	Interface.m_getNumFaces = MikkGetNumFaces;
	Interface.m_getNumVerticesOfFace = MikkGetNumVerticesOfFace;
	Interface.m_getPosition = MikkGetPosition;
	Interface.m_getNormal = MikkGetNormal;
	Interface.m_getTexCoord = MikkGetTexCoord;
	Interface.m_setTSpace = MikkSetTSpace;

	SMikkTSpaceContext Context{};
	Context.m_pInterface = &Interface;
	Context.m_pUserData = &Mikk;
	if (!genTangSpaceDefault(&Context)) {
		return; // 메모리 할당 실패. 기존 Tangent를 그대로 둔다
	}

	// 2. 코너 결과를 정점에 기록. 한 정점의 코너끼리 값이 다르면 정점을 복제해 나눈다
	const int32 OriginalVertexCount{ Vertices.Num() };
	TArray<uint8> bImported{}; // 기록하면서 W가 바뀌므로 시작 전에 표시해 둔다 (TArray<bool>은 vector<bool>이라 못 쓴다)
	bImported.SetNum(OriginalVertexCount);
	for (int32 Index{ 0 }; Index < OriginalVertexCount; ++Index) {
		bImported[Index] = bKeepExisting && Vertices[Index].Tangent.W != 0.0f;
	}
	TArray<uint8> bAssigned{};
	bAssigned.SetNum(OriginalVertexCount);
	TArray<TArray<uint32>> Splits{}; // 원래 정점 → 복제한 정점들
	Splits.SetNum(OriginalVertexCount);

	for (int32 Corner{ 0 }; Corner < IndexCount; ++Corner) {
		const uint32 VertexIndex{ Indices[Corner] };
		if (bImported[VertexIndex]) {
			continue; // 임포트한 Tangent
		}

		const FVector4& Tangent{ Mikk.CornerTangents[Corner] };
		if (!bAssigned[VertexIndex]) {
			Vertices[VertexIndex].Tangent = Tangent;
			bAssigned[VertexIndex] = 1;
			continue;
		}
		if (IsSameTangent(Vertices[VertexIndex].Tangent, Tangent)) {
			continue;
		}

		int32 SplitIndex{ -1 };
		for (uint32 Candidate : Splits[VertexIndex]) {
			if (IsSameTangent(Vertices[Candidate].Tangent, Tangent)) {
				SplitIndex = static_cast<int32>(Candidate);
				break;
			}
		}
		if (SplitIndex < 0) {
			SplitIndex = Vertices.Num();
			FVertex Copy{ Vertices[VertexIndex] };
			Copy.Tangent = Tangent;
			Vertices.Add(Copy);
			if (!LODSourceVertices.IsEmpty()) {
				LODSourceVertices.Add(LODSourceVertices[VertexIndex]);
			}
			Splits[VertexIndex].Add(static_cast<uint32>(SplitIndex));
		}
		Indices[Corner] = static_cast<uint32>(SplitIndex);
	}

	bTangentsBuilt = true;
}