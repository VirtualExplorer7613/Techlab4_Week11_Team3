#pragma once

#include <iosfwd>

#include "Core/Types.h"
#include "Core/EngineString.h"

struct FStaticMeshData;

// Cook이 끝난 FStaticMeshData를 .bin으로 저장하고 읽는다.
class FStaticMeshBake
{
public:
	static TUniquePtr<FStaticMeshData> ReadBaked(const FString& BinPath);
	static void WriteBaked(const FString& BinPath, const FStaticMeshData& Data);

	// 헤더 없이 메시 본문만 (정점, LOD 원본 정점, 인덱스, Section, 머티리얼 슬롯, AABB). FBX Bake도 같은 형식을 쓴다.
	// TextureBaseDir가 있으면 텍스처 경로를 그 폴더 기준 상대 경로로 저장하고, 읽을 때 다시 붙인다.
	static bool ReadMeshBody(std::ifstream& In, FStaticMeshData& Data, uint32 MaxCount, const FString& TextureBaseDir = FString());
	static void WriteMeshBody(std::ofstream& Out, const FStaticMeshData& Data, const FString& TextureBaseDir = FString());
};
