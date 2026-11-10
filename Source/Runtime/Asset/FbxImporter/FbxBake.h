#pragma once

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Core/Types.h"
#include "Core/EngineString.h"

struct FSkeletalMeshData;

struct FFbxAnimBakeData
{
	int32 NumFrames = 0;
	TArray<FBoneAnimationTrack> Tracks;
};

class FFbxBake
{
public:
	// <원본>.bin에는 스태틱 메시와 스켈레탈 메시를 같은 형식으로 저장한다.
	static TUniquePtr<FSkeletalMeshData> Read(const FString& BinPath, const FString& SourcePath);
	static void Write(const FString& BinPath, const FString& SourcePath, const FSkeletalMeshData& Data);

	static bool GetSourceTime(const FString& SourcePath, int64& OutTime);
	static TUniquePtr<FFbxAnimBakeData> ReadAnimation(const FString& BinPath, int64 SourceTime, int32 StackIndex, int32 SampleRate);
	static void WriteAnimation(const FString& BinPath, int64 SourceTime, int32 StackIndex, int32 SampleRate, const FFbxAnimBakeData& Data);
};
