#pragma once

#include "RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Texture2D.h"
#include "Text/Font.h"
#include "SceneView.h"
#include "Occlusion/GPUOcclusion.h"
#include "Engine/LightSceneProxy.h"
#include "Render/Buffer.h"
#include "MeshPassProcessor.h"
#include <span>

class FSkyboxRenderer;
class FHeightFogRenderer;
class FFXAARenderer;
class FCameraOverlayRenderer;

class FTextRenderer;

class FRenderer
{
public:
	// 전방 선언한 패스 객체를 TUniquePtr로 소유하므로 정의는 cpp에 둔다.
	FRenderer();
	~FRenderer();

	bool Init();

	// 패스의 의미를 해석하지 않고 준비된 GPU 상태와 드로우만 제출한다.
	void UploadViewMatrix(const FMatrix& ViewProjection);
	void DrawMeshes(std::span<const FMeshDrawBatch> Batches);

	// 오클루전 객체 조회
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 측정 대상 드로우를 동일한 GPU 상태로 다시 제출한다.
	FOcclusionMeasureResult MeasureDrawVisibility(std::span<const FMeshDrawBatch> Batches);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

	void InitScenePasses(const FString& SkyPanoramaPath);

	// FSceneRenderer::Render가 쓰는 상주 패스 객체. UE에서는 렌더러 모듈의 글로벌 자원에 해당한다.
	FSkyboxRenderer* GetSkybox() const { return Skybox.get(); }
	FHeightFogRenderer* GetHeightFog() const { return HeightFog.get(); }
	FFXAARenderer* GetFXAA() const { return FXAA.get(); }
	FCameraOverlayRenderer* GetCameraOverlay() const { return CameraOverlay.get(); }
	FTextRenderer* GetSceneText() const { return SceneText.get(); }

	FConstantBuffer* GetToneMapConstants() const { return ToneMapConstants.get(); }
	FConstantBuffer* GetDecalConstants() const { return DecalConstants.get(); }
	
private:
	void EnsurePerObjectSlotCapacity(uint32 SlotCount);
	void BindBatch(const FMeshDrawBatch& Batch);
	void BindObject(const FMeshDrawBatch& Batch, const FMeshDrawBatch::FItem& Item);
	TUniquePtr<FConstantBuffer> ObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;
	TUniquePtr<FConstantBuffer> MaterialCB;
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	TUniquePtr<FConstantBuffer> ToneMapConstants;
	TUniquePtr<FConstantBuffer> DecalConstants;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;
	FGPUOcclusion GPUOcclusion;

	TUniquePtr<FSkyboxRenderer> Skybox;
	TUniquePtr<FHeightFogRenderer> HeightFog;
	TUniquePtr<FFXAARenderer> FXAA;
	TUniquePtr<FCameraOverlayRenderer> CameraOverlay;
	TUniquePtr<FTextRenderer> SceneText;
};
