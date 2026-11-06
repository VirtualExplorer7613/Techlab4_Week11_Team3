#include "EnginePCH.h"
#include "SkeletalMeshSceneProxy.h"
#include "Components/SkinnedMeshComponent.h"

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkinnedMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent), SkinnedComponent(InComponent)
{
}

void FSkeletalMeshSceneProxy::UpdateRenderState()
{
	// 표시·그림자 플래그는 공통 처리. 스켈레탈 컴포넌트는 GetRenderMesh()가 nullptr이라 섹션은 여기서 채운다
	FPrimitiveSceneProxy::UpdateRenderState();

	const USkeletalMesh* SkeletalMesh = SkinnedComponent->GetSkeletalMesh();
	if (!SkeletalMesh)
		return;

	TSharedPtr<FMeshRenderState> State = MakeShared<FMeshRenderState>();
	State->LODCount = 1;
	for (const FStaticMeshSection& Section : SkeletalMesh->Data.Mesh.Sections)
	{
		// 컴포넌트의 덮어쓰기 머티리얼이 있으면 그것, 없으면 에셋 기본값
		if (UMaterial* Material = SkinnedComponent->GetMaterial(static_cast<int32>(Section.MaterialSlotIndex)))
			State->Sections.Add({ Material, Section.StartIndex, Section.IndexCount });
	}
	State->LODs[0] = { 0, static_cast<uint32>(State->Sections.Num()) };
	RenderState = State;
}

FVertexBuffer* FSkeletalMeshSceneProxy::GetVertexBuffer(uint32 LODIndex) const
{
	const FSkeletalMeshObjectCPUSkin* MeshObject = SkinnedComponent->GetMeshObject();
	return LODIndex == 0 && MeshObject ? MeshObject->GetVertexBuffer() : nullptr;
}

FIndexBuffer* FSkeletalMeshSceneProxy::GetIndexBuffer(uint32 LODIndex) const
{
	const USkeletalMesh* SkeletalMesh = SkinnedComponent->GetSkeletalMesh();
	return LODIndex == 0 && SkeletalMesh ? SkeletalMesh->IndexBuffer.get() : nullptr;
}
