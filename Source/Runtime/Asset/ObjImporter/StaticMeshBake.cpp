#include "EnginePCH.h"
#include "StaticMeshBake.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <type_traits>

#include "Asset/BakeArchive.h"
#include "Render/StaticMeshData.h"
#include "Core/EngineLog.h"

static_assert(sizeof(FVertex) == 64);

// 바이트 그대로 읽고 쓰는 타입. 복사 생성자를 직접 작성하면 여기서 컴파일 오류가 난다
static_assert(std::is_trivially_copyable_v<FVertex>);
static_assert(std::is_trivially_copyable_v<FStaticMeshSection>);
static_assert(std::is_trivially_copyable_v<FVector4>);
static_assert(std::is_trivially_copyable_v<FBox>);

static_assert(std::is_trivially_copyable_v<FLODSourceVertex>);

namespace
{
	// FStaticMeshData, FVertex, FStaticMeshSection, FStaticMaterialSlot 구조가 바뀌면 올린다
	constexpr uint32 BakeVersion{ 5 };

	struct FBakeHeader
	{
		// 'S'taic 'M'esh 'B'a'K'e
		char Magic[4] = { 'S', 'M', 'B', 'K' };
		uint32 Version = BakeVersion;
	};

	using namespace BakeArchive;

	// 텍스처 경로를 기준 폴더 상대로 바꿔 저장한다 (다른 PC에서도 맞게). 기준이 없거나 바꿀 수 없으면 그대로
	FString ToStoredPath(const FString& Path, const FString& BaseDir)
	{
		if (Path.empty() || BaseDir.empty())
			return Path;
		const std::filesystem::path Relative = std::filesystem::path(Path).lexically_relative(BaseDir);
		return Relative.empty() ? Path : Relative.generic_string();
	}

	FString FromStoredPath(const FString& Path, const FString& BaseDir)
	{
		if (Path.empty() || BaseDir.empty() || std::filesystem::path(Path).is_absolute())
			return Path;
		return (std::filesystem::path(BaseDir) / Path).lexically_normal().generic_string();
	}
}

bool FStaticMeshBake::ReadMeshBody(std::ifstream& In, FStaticMeshData& Data, uint32 MaxCount, const FString& TextureBaseDir)
{
	if (!ReadArray<FVertex>(In, Data.Vertices, MaxCount) ||
		!ReadArray<FLODSourceVertex>(In, Data.LODSourceVertices, MaxCount) ||
		!ReadArray<uint32>(In, Data.Indices, MaxCount) ||
		!ReadArray<FStaticMeshSection>(In, Data.Sections, MaxCount))
	{
		return false;
	}

	uint32 MaterialSlotsNum = 0;
	if (!ReadPod<uint32>(In, MaterialSlotsNum))
	{
		return false;
	}

	for (uint32 i = 0; i < MaterialSlotsNum; ++i)
	{
		FStaticMaterialSlot Slot{};
		if (!ReadString(In, Slot.Name) || !ReadPod<FVector4>(In, Slot.BaseColor) || !ReadString(In, Slot.DiffuseTexturePath) || !ReadString(In, Slot.NormalTexturePath)) {
			return false;
		}
		Slot.DiffuseTexturePath = FromStoredPath(Slot.DiffuseTexturePath, TextureBaseDir);
		Slot.NormalTexturePath = FromStoredPath(Slot.NormalTexturePath, TextureBaseDir);
		Data.MaterialSlots.Add(std::move(Slot));
	}

	// AABB
	return ReadPod<FBox>(In, Data.AABB);
}

void FStaticMeshBake::WriteMeshBody(std::ofstream& Out, const FStaticMeshData& Data, const FString& TextureBaseDir)
{
	WriteArray<FVertex>(Out, Data.Vertices);
	WriteArray<FLODSourceVertex>(Out, Data.LODSourceVertices);
	WriteArray<uint32>(Out, Data.Indices);
	WriteArray<FStaticMeshSection>(Out, Data.Sections);

	// MaterialSlots
	WritePod<uint32>(Out, Data.MaterialSlots.Num());
	for (const auto& Slot : Data.MaterialSlots)
	{
		WriteString(Out, Slot.Name);
		WritePod<FVector4>(Out, Slot.BaseColor);
		WriteString(Out, ToStoredPath(Slot.DiffuseTexturePath, TextureBaseDir));
		WriteString(Out, ToStoredPath(Slot.NormalTexturePath, TextureBaseDir));
	}

	// AABB
	WritePod<FBox>(Out, Data.AABB);
}

TUniquePtr<FStaticMeshData> FStaticMeshBake::ReadBaked(const FString& BinPath)
{
	std::ifstream In(BinPath, std::ios::binary);
	if (!In)
	{
		return nullptr;   // .bin이 없음 (처음 로드)
	}

	TUniquePtr<FStaticMeshData> Data = MakeUnique<FStaticMeshData>();
	uint32 MaxCount = 1000000;

	// 우리 형식이 맞는지, 같은 구조로 저장했는지 확인
	FBakeHeader Header;
	const FBakeHeader Expected;
	if (!ReadPod<FBakeHeader>(In, Header) ||
		std::memcmp(Header.Magic, Expected.Magic, sizeof(Header.Magic)) != 0 ||
		Header.Version != Expected.Version)
	{
		HTR_LOG(Warning, "[BAKE] {}: invalid header or version, re-cooking", BinPath);
		return nullptr;
	}

	if (!ReadMeshBody(In, *Data, MaxCount))
	{
		return nullptr;
	}

	// 바이트 수는 맞아도 내용이 깨졌을 수 있으므로 GPU에 올리기 전에 검사
	FString Error;
	if (!Data->Validate(Error))
	{
		HTR_LOG(Warning, "[BAKE] {}: {}, re-cooking", BinPath, Error);
		return nullptr;
	}

	HTR_LOG(Info, "[BAKE] {}: loaded", BinPath);
	return Data;
}

void FStaticMeshBake::WriteBaked(const FString& BinPath, const FStaticMeshData& Data)
{
	std::ofstream Out(BinPath, std::ios::binary);
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: cannot open for writing", BinPath);
		return;
	}

	WritePod<FBakeHeader>(Out, FBakeHeader());
	WriteMeshBody(Out, Data);

	// 스트림은 한 번 실패하면 상태가 유지되므로 마지막에 한 번만 확인
	if (!Out)
	{
		HTR_LOG(Warning, "[BAKE] {}: write failed", BinPath);
	}
}
