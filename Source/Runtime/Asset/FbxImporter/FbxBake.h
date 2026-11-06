#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"

struct FSkeletalMeshData;

// FBX Import 결과(FSkeletalMeshData)를 <원본>.bin으로 저장하고 읽는다.
// 뼈가 없는 FBX(StaticMesh)도 같은 형식으로 저장한다 (헤더의 AssetType으로 구분).
class FFbxBake
{
public:
	// .bin이 있고 형식·버전이 맞고 원본 FBX 수정 시간이 저장 당시와 같으면 읽는다. 아니면 nullptr (다시 Import)
	static TUniquePtr<FSkeletalMeshData> Read(const FString& BinPath, const FString& SourcePath);
	static void Write(const FString& BinPath, const FString& SourcePath, const FSkeletalMeshData& Data);
};
