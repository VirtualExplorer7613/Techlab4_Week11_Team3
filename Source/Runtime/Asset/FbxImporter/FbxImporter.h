#pragma once

#include "Core/EngineString.h"
#include "Core/Types.h"

struct FSkeletalMeshData;
class UAnimSequence;

class FFbxImporter
{
public:
	static TUniquePtr<FSkeletalMeshData> LoadSkeletalMeshData(const FString& Path);

	static void PrintFbxInfo(const FString& Path);
	static void PrintSkeleton(const FSkeletalMeshData& Data);
	static UAnimSequence* LoadAnimSequence(const FString& Path,	int32 StackIndex = 0, int32 SampleRate = 30);
private:

};