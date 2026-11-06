#pragma once

#include "Math/Vector4.h"
#include "Math/Rect.h"
#include "Core/EngineString.h"
#include "Engine/SceneManagement.h"
#include "Math/Frustum.h"
#include "Containers/Array.h"
#include <cstddef>

// CPU의 그림자 그리기와 GPU 업로드에서 공유하는 데이터.
struct FLightShadowRange // Detail 창에서 띄우려면 필요
{
	uint32 StartIndex = 0; 
	uint32 ViewCount = 0; 
};

static_assert(sizeof(FLightShadowRange) == 8);

// UberLit.hlsl의 FAmbientLightInfo와 같은 배치다.
struct FAmbientLightRenderData
{
	FVector4 ColorIntensity;
};

// Sky Light 설정의 씬 스냅샷. SourcePath로 캐시된 irradiance 큐브맵을 찾는다.
struct FSkyLightRenderData
{
	FVector4 ColorIntensity;
	FString SourcePath;
};

// UberLit.hlsl의 b1 조명 상수 버퍼와 같은 배치다.
struct FForwardLightingConstants
{
	FAmbientLightRenderData Ambient;
	FVector4 CameraPosition;
	uint32 LightCounts[4];  // x: 포인트 개수, y: 스포트 개수, z: 방향광 개수, w: 렉트 개수
	uint32 LightOffsets[4]; // 공유 조명 버퍼에서 각 종류가 시작되는 위치, 순서는 위와 같음
	// 그림자는 광원마다 FLightRenderParameters::ShadowMapChannel로 ScreenShadowMask에서 읽는다.
	FVector4 SkyColorIntensity;
};

// CPU 전용 그림자 뷰 설명 (Details 미리보기, 캐스케이드 경계 선, stat). 그림자 맵 한 장(Point 면, 캐스케이드)마다 하나. GPU에 올리지 않는다.
struct FLightShadowRenderData
{
	FMatrix ViewProjection;
	FRect AtlasRect;
	uint32 AtlasIndex;
	float Near;
	float Far;
	float WorldUnitsPerTexel;
	float Bias;      // 빛 방향 오프셋(월드)
	float SlopeBias; // 법선 방향 오프셋(월드), 빛과의 각도에 비례
	float Sharpen;   // 0~1, PCF 결과의 반그림자 폭을 좁힌다
};

class ULightComponent;

// 광원 컴포넌트의 그림자 설정을 렌더 스레드 쪽으로 옮긴 사본. VisibleLights와 같은 순서로 둔다. GPU에 올리지 않는다.
struct FLightShadowSettings
{
	const ULightComponent* Light = nullptr; // 그림자 캐시가 프레임 사이에 같은 광원인지 비교하는 키
	bool bCastShadows = true;
	float ResolutionScale = 1.0f;
	float Bias = 0.0f;
	float SlopeBias = 0.0f;
	float Sharpen = 0.0f;
};

// 그림자 캐시 무효화 기록. 그림자를 드리우는 프리미티브가 움직이거나 생기거나 사라지면 그 영역을 남긴다.
// Version은 FScene이 기록할 때마다 1씩 올리는 번호다. 뷰는 마지막으로 처리한 번호 이후의 기록만 본다.
struct FShadowInvalidation
{
	uint64 Version = 0;
	FAABB Bounds{};
};

// FScene의 무효화 기록을 FSceneRenderData::Commit에 넘기는 읽기 전용 묶음.
struct FShadowInvalidationList
{
	const TArray<FShadowInvalidation>* Records = nullptr;
	uint64 OldestVersion = 0;  // 이 번호 이하의 기록은 버려졌다. 이보다 뒤처진 뷰는 전부 다시 그린다.
	uint64 CurrentVersion = 0; // 지금까지 기록된 마지막 번호
};

static_assert(sizeof(FVector4) == 16);
static_assert(sizeof(FLightShadowRenderData) == 108);
static_assert(offsetof(FLightShadowRenderData, WorldUnitsPerTexel) == 92);
static_assert(sizeof(FAmbientLightRenderData) == 16);
static_assert(offsetof(FForwardLightingConstants, CameraPosition) == 16);
static_assert(offsetof(FForwardLightingConstants, LightCounts) == 32);
static_assert(offsetof(FForwardLightingConstants, LightOffsets) == 48);
static_assert(offsetof(FForwardLightingConstants, SkyColorIntensity) == 64);
static_assert(sizeof(FForwardLightingConstants) == 80);
