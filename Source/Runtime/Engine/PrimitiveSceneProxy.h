#pragma once

#include "Math/Frustum.h"
#include "Render/MeshRenderData.h"

class FScene;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterial;
class FVertexBuffer;
class FIndexBuffer;

// 기본 프록시는 스태틱 메시(에셋 버퍼를 그대로 그림)용이다. 다른 메시 종류는 파생해 버퍼·섹션을 바꾼다.
class FPrimitiveSceneProxy
{
public:
	explicit FPrimitiveSceneProxy(UPrimitiveComponent* InComponent);

	virtual ~FPrimitiveSceneProxy();
	void UpdateTransform();
	virtual void UpdateRenderState();

	// 그림자·외곽선처럼 프록시 단위로 그리는 패스가 쓰는 버퍼. 기본은 스태틱 메시 에셋의 LOD 버퍼
	virtual FVertexBuffer* GetVertexBuffer(uint32 LODIndex) const;
	virtual FIndexBuffer* GetIndexBuffer(uint32 LODIndex) const;
	void OnMeshRenderDataChanged();
	void OnMeshDestroyed();

	UPrimitiveComponent* GetComponent() const { return Component; }
	const FAABB& GetBounds() const { return Bounds; }
	const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
	const FMatrix& GetWorldToLocal() const { return WorldToLocal; }
	const FLODSphere& GetLODSphere() const { return LODSphere; }
	UStaticMesh* GetMesh() const { return Mesh; }
	bool IsVisible() const { return bVisible; }
	bool IsShown(bool bGameView) const { return bVisible && !(bGameView && bHiddenInGame); }	uint8 GetLODCount() const { return RenderState ? RenderState->LODCount : 1; }
	const FMeshRenderState* GetRenderState() const { return RenderState.get(); }
	const float* GetLODThresholdsSq() const { return RenderState->LODThresholdSq; }
	const FCachedMeshLOD& GetLOD(uint32 Index) const { return RenderState->LODs[Index]; }
	const FCachedMeshSection& GetSection(uint32 Index) const { return RenderState->Sections[Index]; }
	FScene* GetScene() const { return Scene; }

	void SetCastShadow(bool NewValue);
	void SetReceiveShadow(bool NewValue);

	bool bCastsShadow = true;
	bool bReceiveShadow = true;
	bool bReceiveDecals = true;
protected:
	// 섹션(머티리얼·인덱스 범위)과 LOD. 파생 프록시가 UpdateRenderState에서 채운다
	TSharedPtr<const FMeshRenderState> RenderState;
private:
	friend class FScene;
	FScene* Scene = nullptr;
	UPrimitiveComponent* Component = nullptr;
	UStaticMesh* Mesh = nullptr;
	FMatrix LocalToWorld;
	FMatrix WorldToLocal;
	FAABB Bounds;
	FLODSphere LODSphere;
	int32 PackedIndex = INDEX_NONE;
	bool bQueuedForUpdate = false;
	bool bRenderStateQueued = false;
	bool bVisible = true;
	bool bHiddenInGame = false;
};
