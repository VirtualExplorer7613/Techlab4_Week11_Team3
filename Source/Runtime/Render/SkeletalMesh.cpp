#include "EnginePCH.h"
#include "SkeletalMesh.h"
#include "Asset/AssetManager.h"
#include "Render/Material.h"

UMaterial* USkeletalMesh::GetMaterial(uint32 SlotIndex) const
{
	if (SlotIndex < static_cast<uint32>(Materials.Num()) && Materials[SlotIndex])
		return Materials[SlotIndex];
	return UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
}
