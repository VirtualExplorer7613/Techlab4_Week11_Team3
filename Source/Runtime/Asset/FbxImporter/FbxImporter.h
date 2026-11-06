#pragma once

#include "Core/EngineString.h"
#include "Core/Types.h"

struct FSkeletalMeshData;

class FFbxImporter
{
public:
	static TUniquePtr<FSkeletalMeshData> LoadSkeletalMeshData(const FString& Path);

	static void PrintFbxInfo(const FString& Path);
	static void PrintSkeleton(const FSkeletalMeshData& Data);
private:

};