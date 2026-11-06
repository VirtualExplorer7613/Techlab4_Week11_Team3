#pragma once

#include "PrimitiveSceneProxy.h"
#include "LightSceneProxy.h"
#include "Components/BillboardComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Math/Frustum.h"
#include "Math/BVH.h"
#include "Render/Renderer.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/LightRenderData.h"
#include "Render/LightSceneInfo.h"
#include <unordered_map>

class FScene
{
public:
	void AddPrimitive(UPrimitiveComponent* Component);
	void RemovePrimitive(UPrimitiveComponent* Component);
	// 모든 프록시를 한 번에 지운다. 액터를 통째로 지우기 전(ClearWorld)에 불러야 지워진 컴포넌트를 가리키는 프록시가 남지 않는다.
	void RemoveAllPrimitives();

	void AddLight(ULightComponent* Component);
	void RemoveLight(ULightComponent* Component);

	void RemoveAllLights();

	const class FAmbientLightSceneProxy* GetActiveAmbientLight(bool bGameView) const { return nullptr; }
	FAmbientLightRenderData GetSkyAmbientLight(bool bGameView) const;
	void PrepareSkyLight(const FSkyLightRenderData& SkyLight);
	const FSkyLightEnvironment* FindSkyLightEnvironment(const FString& SourcePath) const;
	FSkyLightEnvironment* FindPendingSkyLightEnvironment(const FString& SourcePath);

	const TArray<FDirectionalLightSceneProxy*>& GetDirectionalLights() const { return DirectionalLightProxies; }
	// Point·Spot·Rect를 한 배열로 다룬다. 종류는 GetLightShaderParameters가 채우는 Type으로 구분한다.
	const TArray<FLocalLightSceneProxy*>& GetLocalLights() const { return LocalLightProxies; }

	void UpdateAllTransforms();

	void BuildBVH();

	void MarkDirty(FPrimitiveSceneProxy* Proxy);
	void MarkRenderStateDirty(FPrimitiveSceneProxy* Proxy);

	// 그림자 캐시 무효화 기록. 뷰마다 처리한 Version이 다르므로 지우지 않고 쌓다가 한도를 넘으면 오래된 절반을 버린다.
	FShadowInvalidationList GetShadowInvalidations() const
	{
		return { &ShadowInvalidations, ShadowOldestVersion, ShadowVersion };
	}

	TArray<FPrimitiveSceneProxy*> PrimitiveSceneProxies;
	TArray<FPrimitiveSceneProxy*> DirtyProxies;
	TArray<FPrimitiveSceneProxy*> RenderStateDirtyProxies;
	TArray<FAABB> PrimitiveBounds;
	TArray<uint8> PrimitiveFlags;

	// 컬링 결과로 프록시를 바로 내보내 컴포넌트를 역참조하지 않는다. 경계는 Build/Refit 때만 계산한다.
	TBVH<FPrimitiveSceneProxy*> BVH{
		[](const FPrimitiveSceneProxy* Proxy) -> FBox
		{
			const FAABB& B = Proxy->GetBounds();
			return FBox{ B.Center - B.Extent, B.Center + B.Extent };
		}
	};
	bool bElementListChanged = false;

	// 그림자 렌더러가 쓰는 라이트 목록. Id가 인덱스이고, 지워진 칸은 nullptr로 남겨 다음 라이트가 재사용한다.
	TArray<FLightSceneInfo*>              Lights;
	TArray<FLocalLightSceneProxy*>        LocalLightProxies;      // ↔ PackedIndex ↔
	TArray<FDirectionalLightSceneProxy*>  DirectionalLightProxies;
	TArray<USkyLightComponent*>           SkyLights;               // 프록시 없음

private:
	void RecordShadowInvalidation(const FAABB& Bounds);
	void InvalidateAllShadows();

	static constexpr int32 MaxShadowInvalidations = 4096;
	TArray<FShadowInvalidation> ShadowInvalidations;
	uint64 ShadowVersion = 0;
	uint64 ShadowOldestVersion = 0;

	std::unordered_map<FString, TUniquePtr<FSkyLightEnvironment>> SkyLightEnvironments;
};
