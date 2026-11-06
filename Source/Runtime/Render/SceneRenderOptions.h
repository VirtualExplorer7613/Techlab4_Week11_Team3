#pragma once

#include "Render/RenderEnums.h"

// 뷰 하나의 씬 렌더링 옵션. 에디터와 게임 뷰포트가 같은 설정을 전달한다.
struct FSceneRenderOptions
{
	bool bDrawPrimitives = true;
	EPcfMode PcfMode = EPcfMode::Sample8;
	float ShadowDepthBias = 0.0001f;
	bool bCacheShadows = true; // r.Shadow.CacheWholeSceneShadows: 바뀌지 않은 그림자 타일은 다시 그리지 않는다
	bool bEnableFog = true;
	bool bEnableFXAA = false;
	ELightCullingMode LightCullingMode = ELightCullingMode::Tiled;
	bool bShowSceneDepth = false;
	bool bShowCascade = false;
	bool bShowShadowMask = false;   // 언리얼 구조 그림자의 ScreenShadowMask를 화면에 띄운다 (Deferred)
	bool bShowWorldNormal = false;
	bool bShowLightCulling = false;
	float SceneDepthRange = 1.0f;
	uint32 DirectionalShadowMapSize = 0;
	uint32 PointShadowMapSize = 0;
	uint32 SpotShadowMapSize = 0;

	bool bGammaCorrection = true;
	float DisplayGamma = 2.2f;
	bool bShowVignette = true;
	bool bShowCameraOverlay = true;
	bool bShowDecals = true;

	ERenderingPath RenderingPath = ERenderingPath::Deferred;
	ELightingModel LightingModel = ELightingModel::Gouraud;
};
