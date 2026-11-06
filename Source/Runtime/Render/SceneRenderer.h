#pragma once

#include "Core/Types.h"
#include "Render/SceneView.h"
#include "Render/RenderPacket.h"
#include "Render/MeshPassProcessor.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneRenderOptions.h"
#include "Render/RenderGraph/RenderGraph.h"

class UWorld;
class FScene;
class FRenderer;
class FConstantBuffer;
class FTexture2D;
class FSkyboxRenderer;
struct FSceneViewTargets;
struct FLightSceneInfo;

// 에디터는 실행할 작업만 제공한다. 패스 생성과 순서는 FSceneRenderer가 결정한다.
struct FSceneOverlayCallbacks
{
	std::function<void()> EditorLines;
	std::function<void()> EditorGrid;
	std::function<void()> EditorOutline;
	std::function<void()> EditorGizmo;
	std::function<void()> SIEOutline;
	std::function<void()> SIEGizmo;
	bool bClearEditorDepth = false;
	std::function<void()> EditorUUID;
	std::function<void()> GameUI;
};

struct FScenePassCallbacks // 기본 도형 다 그려진 후 그려져야 될 목록들
{
	std::function<bool()> HeightFog;
	std::function<FConstantBuffer*(bool)> TranslucentFogConstants;
	std::function<void()> SceneText;
	std::function<void()> DepthVisualization;
	std::function<void()> CascadeVisualization;
	std::function<bool()> FXAA;
	FTexture2D* FXAAOutput = nullptr;
	FSceneOverlayCallbacks Overlays;
};


// 씬 렌더링 조율자
class FSceneRenderer
{
public:
	FSceneRenderer(UWorld* InWorld, const FSceneView& InView, FSceneRenderData& InRenderData);
	~FSceneRenderer() = default;
	FSceneRenderer(const FSceneRenderer&) = delete;
	FSceneRenderer& operator=(const FSceneRenderer&) = delete;

	// 가시성 검사 및 렌더 큐 수집
	void InitViews(FRenderer* Renderer, const FSceneRenderOptions& Options);
	static bool SetCascadeCount(uint32 Count);
	static uint32 GetCascadeCount();
	static bool SetCascadeLogRatio(float Ratio);
	static float GetCascadeLogRatio();

	// 뷰 결과를 하나 유지하는 벤치마크에서도 사용한다.
	static void GatherRenderPackets(FScene& Scene, FSceneRenderData& Data, const FViewContext& View,
		const FFrustumPlanes* Frustum = nullptr, FRenderer* Renderer = nullptr);
	static void PrepareDrawData(FRenderer* Renderer, const FSceneRenderData& Data);
	const FRenderStats& GetRenderStats() const { return RenderData.RenderStats; }

	// 한 뷰의 모든 패스를 정해진 순서로 등록한다.
	bool AddViewportPasses(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
		const FSceneRenderOptions& Options, FSkyboxRenderer* Skybox, const FScenePassCallbacks& Callbacks);
	bool DidCorePassSucceed() const { return bCorePassSucceeded; }
	bool DidFXAAPassSucceed() const { return bFXAAPassSucceeded; }

	bool Render(FRenderer* Renderer, FSceneViewTargets& Targets, const FSceneRenderOptions& Options,
		const std::function<void(FScenePassCallbacks&)>& ExtraPasses = nullptr);

	// 반투명 렌더링
	void RenderTranslucent(FRenderer* Renderer, FConstantBuffer* FogConstants = nullptr);

	// 전체 렌더 큐 조회
	FRenderQueue& GetRenderQueue() { return RenderData.RenderQueue; }
	const FRenderQueue& GetRenderQueue() const { return RenderData.RenderQueue; }

	// 뷰 정보 조회
	const FSceneView& GetView() const { return View; }
	const FSceneRenderData& GetRenderData() const { return RenderData; }
	
private:
	bool AddCorePasses(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
		const FSceneRenderOptions& Options, FSkyboxRenderer* Skybox);
	bool AddWorldNormalPass(FRenderGraphBuilder& Graph, FRenderer* Renderer, FSceneViewTargets& Targets,
		const FSceneRenderOptions& Options);
	// SingleLightIndex < 0: 그림자 없는 광원 전부 + Ambient/Sky. 0 이상: VisibleLights[SingleLightIndex] 하나만 ScreenShadowMask를 곱해 더한다.
	bool BindDeferredLighting(FRenderer* Renderer, uint32 Width, uint32 Height, int32 SingleLightIndex);
	bool BindForwardLighting(FRenderer* Renderer, const FSceneRenderOptions& Options, FTexture2D* ShadowMask);
	void UnbindForwardLighting(FRenderer* Renderer);
	bool IntersectSphereFrustum(const FVector4& PositionRadius, const FPlane (&Planes)[6]) const;

	// ---------- 언리얼 구조 그림자 (ShadowSetup.cpp) ----------
	// 라이트마다 이번 뷰에 필요한 그림자 맵(FProjectedShadowInfo)을 만든다.
	void InitDynamicShadows(const FSceneRenderOptions& Options);
	// Spot 1장, Point 6장
	void CreateWholeSceneProjectedShadow(FLightSceneInfo* LightSceneInfo);
	// Directional CSM
	void AddViewDependentWholeSceneShadowsForView(FLightSceneInfo* LightSceneInfo);
	bool HasVisibleReceiverInBounds(const FLightSceneInfo* LightSceneInfo) const;
	// 그림자마다 Caster를 모으고, Caster가 없는 그림자는 버린다.
	void GatherShadowPrimitives();
	void AddInteractingPrimitives(FScene& Scene, FProjectedShadowInfo& Shadow);
	// 남은 그림자를 깊이 아틀라스(Texture2DArray)에 배치한다.
	void AllocateShadowDepthTargets();

	void RenderShadowDepthMaps(FRenderer* Renderer, FTexture2DArray* Atlas);
	// 그림자 한 장을 현재 렌더 타깃(ScreenShadowMask)의 Channel(0~3)에 그린다. t0 장면 깊이, t1 깊이 아틀라스가 묶여 있어야 한다.
	void RenderShadowProjection(const FProjectedShadowInfo& Shadow, uint32 Channel, uint32 Width, uint32 Height);
	// 광원 하나의 ShadowsToProject를 그린다. Directional은 먼 캐스케이드부터 그려 가까운 캐스케이드가 Fade로 덮는다.
	void RenderShadowProjectionsForLight(const FVisibleLightInfo& VisibleLightInfo, uint32 Channel, uint32 Width, uint32 Height);
	// Shadow Mask 뷰 모드: 그림자 광원을 최대 4개까지 채널 하나씩 그린다 (Directional이 R).
	void RenderShadowMaskVisualizationProjections(uint32 Width, uint32 Height);
	// 그림자 맵이 생긴 광원에 ShadowMapChannel을 주고(Deferred 0, Forward 겹침 기반 0~3) 광원 데이터를 다시 올린다.
	// Details 미리보기·캐스케이드 경계 선·stat이 읽는 LightShadows, CascadeShadowDistances도 여기서 채운다.
	void FinalizeShadowLights(const FSceneRenderOptions& Options);

	// 이번 프레임에 그림자를 Projection할 광원. Deferred는 이 순서대로 광원마다 마스크를 만들어 더한다.
	struct FShadowedLight
	{
		int32 VisibleLightIndex = INDEX_NONE;   // RenderData.VisibleLights 인덱스
		int32 LightId = INDEX_NONE;             // FLightSceneInfo::Id (VisibleLightInfos 인덱스)
		uint32 Channel = 0;                     // ScreenShadowMask 채널
	};
	TArray<FShadowedLight> ShadowedLights;

	bool RenderHeightFog(FRenderer* Renderer, FTexture2D* DepthTarget, FTexture2D* ColorTarget);
	void RenderSceneText(FRenderer* Renderer);
	void RenderDecals(FRenderer* Renderer, const FSceneRenderOptions& Options, FTexture2D* ShadowMask);


	UWorld* World = nullptr;
	FSceneView View;
	FSceneRenderData& RenderData;
	bool bLightUploadSucceeded = false;
	bool bTilesBuilt = false;
	TArray<FMeshDrawBatch> ForwardBatches;
	FViewContext CachedViewContext;
	bool bCorePassSucceeded = true;
	bool bFXAAPassSucceeded = true;
	bool bHasFog = false;
};
