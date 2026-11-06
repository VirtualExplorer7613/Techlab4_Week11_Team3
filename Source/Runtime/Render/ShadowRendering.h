#pragma once

#include "Containers/Array.h"
#include "Math/Sphere.h"
#include "Math/Frustum.h"
#include "Engine/SceneManagement.h"

class FPrimitiveSceneProxy;
struct FLightSceneInfo;

// 그림자 맵 타일 테두리(텍셀). PCF가 옆 타일을 읽지 않게 비워 둔다. 언리얼 SHADOW_BORDER와 같은 값.
inline constexpr uint32 ShadowBorder = 4;
// 깊이 아틀라스 한 장(Texture2DArray 슬라이스 하나)의 크기와 최대 슬라이스 수
inline constexpr uint32 ShadowAtlasSize = 4096;
inline constexpr uint32 MaxShadowAtlasSlices = 4;
inline constexpr float UnrealUnitsToWorld = 0.01f;

// 그림자 콘솔 변수. 이름과 기본값은 언리얼 r.Shadow.* 와 같다. 콘솔 명령은 OutputLogPanel에서 연결한다.
struct FShadowConsoleVariables
{
	// 화면 1픽셀당 그림자 텍셀 수. 광원이 화면에서 클수록 해상도가 올라간다.
	float TexelsPerPixel = 1.27324f;
	// Spot/Point 그림자 한 장의 최대·최소 해상도
	uint32 MaxResolution = 2048;
	uint32 MinResolution = 32;
	// 이 해상도 아래부터 그림자를 서서히 끈다. MinResolution 아래에서는 그림자를 만들지 않는다.
	uint32 FadeResolution = 64;
	float FadeExponent = 0.25f;
	// 캐스케이드 한 장의 해상도
	uint32 MaxCSMResolution = 2048;
	// 카메라에서 본 겉보기 크기(반지름 / 거리)가 이보다 작은 Caster는 그림자를 그리지 않는다.
	float RadiusThreshold = 0.01f;

	// Depth bias (UE r.Shadow.*DepthBias). UpdateShaderDepthBias가 읽는다.
	float CSMDepthBias = 10.0f;
	float CSMSlopeScaleDepthBias = 3.0f;

	float SpotLightDepthBias = 3.0f;
	float SpotLightSlopeDepthBias = 3.0f;
	// UE는 Point를 한 장짜리 큐브맵(하드웨어 깊이)으로 그려 0.02를 쓴다. Week9 Point 면은 Spot과 같은 원근 투영이라 Spot 공식과 Spot 값을 쓴다.
	float PointLightDepthBias = 3.0f;
	float PointLightSlopeScaleDepthBias = 3.0f;

	float ShadowMaxSlopeScaleDepthBias = 1.0f;

	// r.ShadowQuality: 0이면 그림자를 끈다. 1~5는 Projection PCF 단계 (1 하드웨어 1회, 2 3x3, 3 Poisson 16, 4~5 Poisson 32)
	uint32 ShadowQuality = 5;
	// r.Shadow.FilterMethod: 0 PCF, 1 PCSS (Directional·Spot만, Point는 PCF)
	uint32 FilterMethod = 0;
	// r.Shadow.MaxSoftKernelSize: PCSS 커널 반경 상한 (텍셀)
	uint32 MaxSoftKernelSize = 40;
};

extern FShadowConsoleVariables GShadowCVars;

// 그림자 맵 한 장. 프레임마다 FSceneRenderData가 만들고 비운다.
class FProjectedShadowInfo
{
public:
	// 어느 라이트의 그림자인지
	FLightSceneInfo* LightSceneInfo = nullptr;

	FVector PreShadowTranslation;
	FMatrix ShadowView, ShadowProjection, ShadowViewProjection;
	FSphere ShadowBounds{ FVector::ZeroVector, 0.0f };
	FShadowCascadeSettings CascadeSettings;
	float MinSubjectZ = 0.0f;
	float MaxSubjectZ = 0.0f;
	// 화면에서 작아질수록 0으로 줄어드는 그림자 세기 (FadeResolution ~ MinResolution 구간)
	float FadeAlpha = 1.0f;



	// Shadow Map 안의 위치. AllocateShadowDepthTargets가 채운다.
	// X, Y는 테두리를 뺀 실제 그리는 영역의 시작점, Resolution은 그 영역의 크기. 타일 전체는 Resolution + 2 * BorderSize.
	uint32 X = 0, Y = 0, ResolutionX = 0, ResolutionY = 0, BorderSize = 0;
	uint32 AtlasIndex = 0;   // Texture2DArray 슬라이스
	bool bAllocated = false;

	uint32 GetTileSize() const { return ResolutionX + BorderSize * 2; }

	// 종류
	bool bWholeSceneShadow = false, bDirectionalLight = false, bOnePassPointLightShadow = false;

	// Caster. GatherShadowPrimitives가 채운다.
	TArray<FPrimitiveSceneProxy*> DynamicSubjectPrimitives;

	FFrustumPlanes CasterFrustum{};

	// Resolution은 그림자 맵에서 실제로 그리는 크기(테두리 제외)
	void SetupWholeSceneProjection(FLightSceneInfo* InLightSceneInfo,
		const FWholeSceneProjectedShadowInitializer& Initializer,
		uint32 InResolutionX, uint32 InResolutionY, uint32 InBorderSize);
	void AddSubjectPrimitive(FPrimitiveSceneProxy* Proxy) { DynamicSubjectPrimitives.Add(Proxy); }

	float ShaderDepthBias = 0.0f;
	float ShaderSlopeDepthBias = 0.0f;
	float ShaderMaxSlopeDepthBias = 1.0f;

	bool IsWholeSceneDirectionalShadow() const { return bWholeSceneShadow && bDirectionalLight && CascadeSettings.ShadowSplitIndex >= 0; }
	bool IsWholeScenePointLightShadow() const { return bWholeSceneShadow && !bDirectionalLight && CascadeSettings.ShadowSplitIndex >= 0; }

	// Spot·Point는 라이트 공간 X(앞쪽) 거리를 MinSubjectZ~MaxSubjectZ로 정규화한 선형 깊이를 쓴다.
	bool IsPerspective() const { return !bDirectionalLight; }
	// 그림자 카메라 앞 방향(월드). ShadowView의 회전 부분 첫 열 = WorldToLight의 X축.
	FVector GetLightDirection() const { return FVector(ShadowView.M[0][0], ShadowView.M[1][0], ShadowView.M[2][0]); }

	void UpdateShaderDepthBias();
};

struct FShadowDepthPassConstants
{
	FVector4 ShadowParams;      // x DepthBias, y SlopeDepthBias, z MaxSlopeDepthBias, w 1 / (MaxSubjectZ - MinSubjectZ)
	FVector  LightDirection;    // GetLightDirection()
	float    ShadowNear;        // MinSubjectZ
	uint32   bPerspective;      // Spot·Point 1, Directional 0
	float    Padding[3]{};
};
static_assert(sizeof(FShadowDepthPassConstants) == 48);

// ShadowProjection.hlsl b0와 같은 배치. 그림자 한 장을 화면 마스크에 그릴 때 쓴다.
struct FShadowProjectionConstants
{
	FMatrix  InvViewProjection;     // 화면 NDC + 장면 깊이 -> 카메라 기준 위치 (Translated World)
	FMatrix  ShadowViewProjection;  // 카메라 기준 위치 -> 그림자 클립
	FVector4 AtlasRect;             // xy = (X, Y) / ShadowAtlasSize, zw = (ResolutionX, ResolutionY) / ShadowAtlasSize
	FVector4 ViewportSize;          // xy = 화면 크기, zw = 1 / 화면 크기
	FVector  LightPosition;  float ShadowNear;      // Spot·Point 선형 깊이 = (dot(World - LightPosition, LightDirection) - ShadowNear) * InvDepthRange
	FVector  LightDirection; float InvDepthRange;
	FVector  ViewOrigin;     float FadeAlpha;       // 화면 깊이 SceneW = dot(World - ViewOrigin, ViewForward)
	FVector  ViewForward;    float ShadowSharpen;   // Sharpen * 7 + 1 (UE와 같음)
	float    SplitNear, SplitFar, FadePlaneOffset, InvFadePlaneLength;
	uint32   AtlasIndex, bPerspective, bUseFadePlane, PcfQuality;
	uint32   FilterMethod;
	float    LightSourceSize;      // Directional: tan(각반지름), Spot: SourceRadius (월드)
	float    MaxSoftKernelTexels;
	float    WorldTexelScale;      // 텍셀 하나의 월드 크기. Directional은 그대로, Spot·Point는 라이트 공간 거리를 곱해서 쓴다
};
static_assert(sizeof(FShadowProjectionConstants) == 272);
