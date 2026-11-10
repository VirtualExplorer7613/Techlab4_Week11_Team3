#include "EnginePCH.h"
#include "FbxBake.h"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>

#include "Asset/BakeArchive.h"
#include "Asset/ObjImporter/StaticMeshBake.h"
#include "Core/EngineLog.h"
#include "Render/SkeletalMeshData.h"

// 바이트 그대로 읽고 쓰는 타입. FMatrix는 복사 연산자를 직접 정의해 trivially copyable이 아니므로 M(float 16개)만 쓴다
static_assert(std::is_trivially_copyable_v<FSkinWeight>);
static_assert(sizeof(FMatrix::M) == sizeof(float) * 16);
static_assert(std::is_trivially_copyable_v<FVector>);
static_assert(std::is_trivially_copyable_v<FQuat>);

namespace
{
	using namespace BakeArchive;

	// FVertex, FBoneInfo, FSkinWeight, FStaticMeshData(StaticMeshBake 본문) 구조가 바뀌면 올린다
	// 2: Static/Skeletal 판정을 뼈 유무에서 스킨 디포머 유무로 바꿈 (예전 Bake를 버리고 다시 Import)
	// 3: 스킨 디포머가 없는 본 하위 메시를 가장 가까운 부모 본에 연결한다.
	constexpr uint32 FbxBakeVersion{ 3 };

	// FBX는 OBJ보다 커서(DROID 인덱스 약 40만 개) 배열 상한을 넉넉히 둔다
	constexpr uint32 MaxCount{ 16'000'000 };

	enum class EBakeAssetType : uint8
	{
		Static = 0,
		Skeletal = 1,
	};

	struct FFbxBakeHeader
	{
		// 'F'B'X' 'B'ake
		char Magic[4] = { 'F', 'B', 'X', 'B' };
		uint32 Version = FbxBakeVersion;
		EBakeAssetType AssetType = EBakeAssetType::Static;
		uint8 Padding[7] = {};
		int64 SourceTime = 0; // 원본 FBX의 last_write_time
	};

	struct FFbxAnimBakeHeader
	{
		char Magic[4] = { 'F', 'B', 'X', 'A' };
		uint32 Version = 1; // 키 타입이나 본문 형식이 바뀌면 올린다.
		int64 SourceTime = 0;
		int32 StackIndex = 0;
		int32 SampleRate = 0;
		int32 NumFrames = 0;
		uint32 TrackCount = 0;
	};

	FString GetTextureBaseDir(const FString& SourcePath)
	{
		return std::filesystem::path(SourcePath).parent_path().generic_string();
	}

	// 읽은 데이터가 그대로 써도 되는지 검사. 바이트 수는 맞아도 내용이 깨졌을 수 있다
	bool ValidateBaked(const FSkeletalMeshData& Data, EBakeAssetType AssetType, FString& OutError)
	{
		if (!Data.Mesh.Validate(OutError))
			return false;

		if (AssetType == EBakeAssetType::Static)
		{
			if (!Data.Bones.IsEmpty())
			{
				OutError = "static asset has bones";
				return false;
			}
			return true;
		}

		if (Data.Bones.IsEmpty())
		{
			OutError = "skeletal asset has no bones";
			return false;
		}
		if (Data.SkinWeights.Num() != Data.Mesh.Vertices.Num())
		{
			OutError = std::format("skin weights {} != vertices {}", Data.SkinWeights.Num(), Data.Mesh.Vertices.Num());
			return false;
		}
		for (int32 i = 0; i < Data.Bones.Num(); ++i)
		{
			// 부모가 항상 자식보다 앞에 있어야 Component Space를 한 번에 계산할 수 있다
			if (Data.Bones[i].ParentIndex >= i)
			{
				OutError = std::format("bone {} parent {} is not before it", i, Data.Bones[i].ParentIndex);
				return false;
			}
		}
		return true;
	}
}

bool FFbxBake::GetSourceTime(const FString& SourcePath, int64& OutTime)
{
	std::error_code Error;
	const auto Time = std::filesystem::last_write_time(SourcePath, Error);
	if (Error)
		return false;
	OutTime = static_cast<int64>(Time.time_since_epoch().count());
	return true;
}

TUniquePtr<FSkeletalMeshData> FFbxBake::Read(const FString& BinPath, const FString& SourcePath)
{
	std::ifstream In(BinPath, std::ios::binary);
	if (!In)
	{
		return nullptr;   // .bin이 없음 (처음 로드)
	}

	int64 SourceTime = 0;
	if (!GetSourceTime(SourcePath, SourceTime))
	{
		return nullptr;
	}

	// 1. 우리 형식·버전인지, 원본이 그 사이 바뀌지 않았는지
	FFbxBakeHeader Header;
	const FFbxBakeHeader Expected;
	if (!ReadPod<FFbxBakeHeader>(In, Header) ||
		std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 ||
		Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[BAKE] {}: invalid header or version, re-importing", BinPath);
		return nullptr;
	}
	if (Header.SourceTime != SourceTime)
	{
		HTR_LOG(Info, "[BAKE] {}: source FBX changed, re-importing", BinPath);
		return nullptr;
	}

	// 2. 메시 본문 (OBJ Bake와 같은 형식) + Tangent 계산 여부
	TUniquePtr<FSkeletalMeshData> Data = MakeUnique<FSkeletalMeshData>();
	uint8 bTangentsBuilt = 0;
	if (!FStaticMeshBake::ReadMeshBody(In, Data->Mesh, MaxCount, GetTextureBaseDir(SourcePath)) ||
		!ReadPod<uint8>(In, bTangentsBuilt))
	{
		return nullptr;
	}
	// 복원하지 않으면 로드 후 Tangent를 다시 계산해 정점이 나뉘고 SkinWeights와 개수가 어긋난다
	Data->Mesh.bTangentsBuilt = bTangentsBuilt != 0;

	// 3. 뼈와 가중치 (Skeletal만)
	if (Header.AssetType == EBakeAssetType::Skeletal)
	{
		uint32 BoneCount = 0;
		if (!ReadPod<uint32>(In, BoneCount) || BoneCount > MaxCount)
		{
			return nullptr;
		}
		Data->Bones.SetNum(static_cast<int32>(BoneCount));
		for (FBoneInfo& Bone : Data->Bones)
		{
			if (!ReadString(In, Bone.Name) ||
				!ReadPod<int32>(In, Bone.ParentIndex) ||
				!ReadPod(In, Bone.RefLocal.M) ||
				!ReadPod(In, Bone.InvBindPose.M))
			{
				return nullptr;
			}
		}
		if (!ReadArray<FSkinWeight>(In, Data->SkinWeights, MaxCount))
		{
			return nullptr;
		}
	}

	// 4. GPU에 올리기 전에 검사
	FString Error;
	if (!ValidateBaked(*Data, Header.AssetType, Error))
	{
		HTR_LOG(Warning, "[BAKE] {}: {}, re-importing", BinPath, Error);
		return nullptr;
	}
	return Data;
}

void FFbxBake::Write(const FString& BinPath, const FString& SourcePath, const FSkeletalMeshData& Data)
{
	FFbxBakeHeader Header;
	Header.AssetType = Data.Bones.IsEmpty() ? EBakeAssetType::Static : EBakeAssetType::Skeletal;
	if (!GetSourceTime(SourcePath, Header.SourceTime))
	{
		HTR_LOG(Warning, "[BAKE] {}: cannot read source time, skipped", BinPath);
		return;
	}

	std::ofstream Out(BinPath, std::ios::binary);
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: cannot open for writing", BinPath);
		return;
	}

	WritePod<FFbxBakeHeader>(Out, Header);
	FStaticMeshBake::WriteMeshBody(Out, Data.Mesh, GetTextureBaseDir(SourcePath));
	WritePod<uint8>(Out, Data.Mesh.bTangentsBuilt ? 1 : 0);

	if (Header.AssetType == EBakeAssetType::Skeletal)
	{
		WritePod<uint32>(Out, static_cast<uint32>(Data.Bones.Num()));
		for (const FBoneInfo& Bone : Data.Bones)
		{
			WriteString(Out, Bone.Name);
			WritePod<int32>(Out, Bone.ParentIndex);
			WritePod(Out, Bone.RefLocal.M);
			WritePod(Out, Bone.InvBindPose.M);
		}
		WriteArray<FSkinWeight>(Out, Data.SkinWeights);
	}

	// 스트림은 한 번 실패하면 상태가 유지되므로 마지막에 한 번만 확인
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: write failed", BinPath);
	}
}

TUniquePtr<FFbxAnimBakeData> FFbxBake::ReadAnimation(const FString& BinPath, int64 SourceTime, int32 StackIndex, int32 SampleRate)
{
	std::ifstream In(std::filesystem::u8path(BinPath), std::ios::binary | std::ios::ate);
	const std::streamoff FileSize = In.tellg();
	if (FileSize < static_cast<std::streamoff>(sizeof(FFbxAnimBakeHeader)))
		return nullptr;
	In.seekg(0);

	FFbxAnimBakeHeader Header;
	const FFbxAnimBakeHeader Expected;
	if (!ReadPod(In, Header) ||
		std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 ||
		Header.Version != Expected.Version || Header.SourceTime != SourceTime ||
		Header.StackIndex != StackIndex || Header.SampleRate != SampleRate ||
		Header.NumFrames <= 0 || Header.NumFrames == std::numeric_limits<int32>::max() ||
		Header.TrackCount == 0 || Header.TrackCount > static_cast<uint32>(std::numeric_limits<int32>::max()))
	{
		return nullptr;
	}

	const uint32 NumKeys = static_cast<uint32>(Header.NumFrames + 1);
	// 파일에 키 데이터가 들어갈 공간이 있는지 배열을 할당하기 전에 확인한다.
	const uint64 MinTrackBytes = 4 * sizeof(uint32) + uint64{ NumKeys } * (2 * sizeof(FVector) + sizeof(FQuat));
	const uint64 BodyBytes = static_cast<uint64>(FileSize) - sizeof(Header);
	if (Header.TrackCount > BodyBytes / MinTrackBytes)
		return nullptr;

	TUniquePtr<FFbxAnimBakeData> Data = MakeUnique<FFbxAnimBakeData>();
	Data->NumFrames = Header.NumFrames;
	Data->Tracks.SetNum(static_cast<int32>(Header.TrackCount));
	for (FBoneAnimationTrack& Track : Data->Tracks)
	{
		FString BoneName;
		FRawAnimSequenceTrack& Keys = Track.InternalTrack;
		if (!ReadString(In, BoneName) || BoneName.empty() ||
			!ReadArray(In, Keys.PosKeys, NumKeys) || !ReadArray(In, Keys.RotKeys, NumKeys) ||
			!ReadArray(In, Keys.ScaleKeys, NumKeys) ||
			Keys.PosKeys.Num() != static_cast<int32>(NumKeys) || Keys.RotKeys.Num() != static_cast<int32>(NumKeys) ||
			Keys.ScaleKeys.Num() != static_cast<int32>(NumKeys))
		{
			return nullptr;
		}
		Track.Name = FName(BoneName);
	}

	if (static_cast<std::streamoff>(In.tellg()) != FileSize)
		return nullptr;
	return Data;
}

void FFbxBake::WriteAnimation(const FString& BinPath, int64 SourceTime, int32 StackIndex, int32 SampleRate, const FFbxAnimBakeData& Data)
{
	FFbxAnimBakeHeader Header;
	Header.SourceTime = SourceTime;
	Header.StackIndex = StackIndex;
	Header.SampleRate = SampleRate;
	Header.NumFrames = Data.NumFrames;
	Header.TrackCount = static_cast<uint32>(Data.Tracks.Num());

	// 쓰는 도중 다른 로더가 불완전한 캐시를 읽지 않도록 완성 후 교체한다.
	const auto CachePath = std::filesystem::u8path(BinPath);
	static std::atomic<uint64> TempFileCounter{ 0 };
	auto TempPath = CachePath;
	TempPath += std::format(".{}.{}.tmp.bin", ::GetCurrentProcessId(), TempFileCounter.fetch_add(1, std::memory_order_relaxed));
	std::ofstream Out(TempPath, std::ios::binary);
	if (!Out)
	{
		HTR_LOG(Warning, "[FBX ANIM BAKE] {}: cannot open for writing", BinPath);
		return;
	}

	WritePod(Out, Header);
	for (const FBoneAnimationTrack& Track : Data.Tracks)
	{
		WriteString(Out, Track.Name.ToString());
		WriteArray(Out, Track.InternalTrack.PosKeys);
		WriteArray(Out, Track.InternalTrack.RotKeys);
		WriteArray(Out, Track.InternalTrack.ScaleKeys);
	}
	Out.close();

	if (!Out || !::MoveFileExW(TempPath.c_str(), CachePath.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		std::error_code Error;
		std::filesystem::remove(TempPath, Error);
		HTR_LOG(Warning, "[FBX ANIM BAKE] {}: write failed", BinPath);
	}
}
