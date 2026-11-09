#pragma once

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Core/Types.h"

#include <functional>

struct FSkeletalMeshData;
class UAnimSequence;

struct FFbxAnimationClip
{
	int32 StackIndex = 0;
	FString Name;
	double DurationSeconds = 0.0;
	UAnimSequence* Sequence = nullptr;
};

class FFbxImporter
{
public:
	static TUniquePtr<FSkeletalMeshData> LoadSkeletalMeshData(const FString& Path);

	static void PrintFbxInfo(const FString& Path);
	static void PrintSkeleton(const FSkeletalMeshData& Data);
	static UAnimSequence* LoadAnimSequence(const FString& Path,	int32 StackIndex = 0, int32 SampleRate = 30);
	static bool LoadAnimationClips(const FString& Path, TArray<FFbxAnimationClip>& OutClips,
		const std::function<UAnimSequence*(int32)>& FindLoadedClip, int32 SampleRate = 30);
private:

};
