#include "EnginePCH.h"
#include "FbxBake.h"

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>

#include "Asset/BakeArchive.h"
#include "Asset/ObjImporter/StaticMeshBake.h"
#include "Core/EngineLog.h"
#include "Render/SkeletalMeshData.h"

// 바이트 그대로 읽고 쓰는 타입. FMatrix는 복사 연산자를 직접 정의해 trivially copyable이 아니므로 M(float 16개)만 쓴다
static_assert(std::is_trivially_copyable_v<FSkinWeight>);
static_assert(sizeof(FMatrix::M) == sizeof(float) * 16);

namespace
{
	using namespace BakeArchive;

	// FVertex, FBoneInfo, FSkinWeight, FStaticMeshData(StaticMeshBake 본문) 구조가 바뀌면 올린다
	// 2: Static/Skeletal 판정을 뼈 유무에서 스킨 디포머 유무로 바꿈 (예전 Bake를 버리고 다시 Import)
	constexpr uint32 FbxBakeVersion{ 2 };

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

	bool GetSourceTime(const FString& SourcePath, int64& OutTime)
	{
		std::error_code Error;
		const auto Time = std::filesystem::last_write_time(SourcePath, Error);
		if (Error)
			return false;
		OutTime = static_cast<int64>(Time.time_since_epoch().count());
		return true;
	}

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
