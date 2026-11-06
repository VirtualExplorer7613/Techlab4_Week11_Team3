#include "EnginePCH.h"
#include "SkinnedMeshComponent.h"
#include "Engine/SkeletalMeshSceneProxy.h"

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	if (SkeletalMesh == InSkeletalMesh)
		return;

	SkeletalMesh = InSkeletalMesh;
	ClearOverrideMaterials();   // 슬롯 구성이 에셋마다 다르다
	OnSkeletalMeshChanged();
}

void USkinnedMeshComponent::OnSkeletalMeshChanged()
{
	ComponentSpace.Reset();
	RecreateMeshObject();
	MarkRenderStateDirty();
	OnTransformDirty();
}

void USkinnedMeshComponent::Serialize(FStructuredArchive::FRecord Record)
{
	// 불러오기는 리플렉션이 SkeletalMesh 포인터만 바꾼다. 생성자 기본 에셋으로 만든 스키닝 버퍼·자세를 새 에셋 기준으로 다시 만든다
	USkeletalMesh* Previous = SkeletalMesh;
	Super::Serialize(Record);   // 덮어쓰기 머티리얼도 여기서 읽으므로 지우지 않는다

	if (Record.GetUnderlyingArchive().IsLoading() && SkeletalMesh != Previous)
		OnSkeletalMeshChanged();
}

const FStaticMeshData* USkinnedMeshComponent::GetMeshData() const
{
	return SkeletalMesh ? &SkeletalMesh->Data.Mesh : nullptr;
}

FBox USkinnedMeshComponent::CalcLocalBounds() const
{
	if (MeshObject && MeshObject->IsValid())
		return MeshObject->GetLocalBounds();
	return Super::CalcLocalBounds(); // 바인드 자세 AABB
}

int32 USkinnedMeshComponent::GetNumMaterials() const
{
	return SkeletalMesh ? static_cast<int32>(SkeletalMesh->Data.Mesh.MaterialSlots.Num()) : 0;
}

FString USkinnedMeshComponent::GetMaterialSlotName(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= GetNumMaterials())
		return FString();
	return SkeletalMesh->Data.Mesh.MaterialSlots[SlotIndex].Name;
}

UMaterial* USkinnedMeshComponent::GetDefaultMaterial(int32 SlotIndex) const
{
	return SkeletalMesh ? SkeletalMesh->GetMaterial(static_cast<uint32>(SlotIndex)) : nullptr;
}

FPrimitiveSceneProxy* USkinnedMeshComponent::CreatePrimitiveSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

void USkinnedMeshComponent::CreateRenderState()
{
	// 프록시가 처음 그릴 때부터 정점 버퍼가 있도록 먼저 만든다
	if (!MeshObject)
		RecreateMeshObject();
	Super::CreateRenderState();
}

void USkinnedMeshComponent::RecreateMeshObject()
{
	MeshObject.reset();
	if (SkeletalMesh)
		MeshObject = MakeUnique<FSkeletalMeshObjectCPUSkin>(SkeletalMesh);
}

// 스태틱 메시는 섹션을 정적 묶음으로 모아 그리지만, 스켈레탈 메시는 컴포넌트마다 정점 버퍼가 달라
// 언리얼의 동적 메시 요소처럼 매 프레임 패킷으로 제출한다.
void USkinnedMeshComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue, const FViewContext& ViewContext)
{
	const FPrimitiveSceneProxy* Proxy = GetSceneProxy();
	const FMeshRenderState* State = Proxy ? Proxy->GetRenderState() : nullptr;
	if (!State || !MeshObject || !MeshObject->IsValid() || !SkeletalMesh)
		return;

	const FCachedMeshLOD& LOD = State->LODs[0];
	for (uint32 I = 0; I < LOD.NumSections; ++I)
	{
		const FCachedMeshSection& Section = State->Sections[LOD.FirstSection + I];
		FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
		Packet.Proxy = Proxy;
		Packet.VertexBuffer = MeshObject->GetVertexBuffer();
		Packet.IndexBuffer = SkeletalMesh->IndexBuffer.get();
		Packet.Material = Section.Material;
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
	}
}

void USkinnedMeshComponent::RefreshSkinning(const TArray<FMatrix>& LocalPose)
{
	if (!SkeletalMesh)
		return;
	if (!MeshObject)
		RecreateMeshObject();

	const TArray<FBoneInfo>& Bones = SkeletalMesh->Data.Bones;
	if (LocalPose.Num() != Bones.Num())
	{
		HTR_LOG(Warning, "Skinning skipped: pose {}, bones {}", LocalPose.Num(), Bones.Num());
		return;
	}

	// 1. Component Space: 부모부터 순서대로 C = L × C(부모)
	ComponentSpace.SetNum(Bones.Num());
	TArray<FMatrix> SkinMatrices;
	SkinMatrices.SetNum(Bones.Num());
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		const int32 Parent = Bones[i].ParentIndex;
		ComponentSpace[i] = Parent >= 0 ? LocalPose[i] * ComponentSpace[Parent] : LocalPose[i];

		// 2. Skinning Matrix: 바인드 자세 지우고 지금 자세 입히기
		SkinMatrices[i] = Bones[i].InvBindPose * ComponentSpace[i];
	}

	// 3. 정점 변형과 GPU 업로드는 MeshObject가 한다
	MeshObject->Update(SkinMatrices);

	// 4. 자세가 바뀌면 바운드도 바뀐다 (컬링·그림자 캐시 갱신)
	OnTransformDirty();
}
