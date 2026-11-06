#pragma once

#include "Components/LightComponent.h"
#include "SceneManagement.h"
#include "Render/LightRenderData.h"
#include "Render/LightSceneInfo.h"
#include "Render/SceneView.h"
#include "Math/Sphere.h"

// MSVC 최적화
// 가상함수 테이블과 패딩 방지
class FLightSceneProxyFixLayout
{
public:
	virtual ~FLightSceneProxyFixLayout() = default;
};

class FLightSceneProxy : public FLightSceneProxyFixLayout
{
public:
	FLightSceneProxy(const ULightComponent* InLightComponent)
		: LightComponent(InLightComponent) {}
	virtual ~FLightSceneProxy() = default;

	virtual void GetLightShaderParameters(FLightRenderParameters& OutParameters) const = 0;

	const ULightComponent* GetLightComponent() const { return LightComponent; }
	bool IsVisible() const { return LightComponent ? LightComponent->IsVisible() : false; }
	bool IsEnabled() const { return LightComponent && LightComponent->GetIntensity() > 0.0f; }
	bool IsShown(bool bGameView) const { return LightComponent ? LightComponent->IsShown(bGameView) : false; }

	// 컴포넌트 값을 매 프레임 읽는다. 에디터에서 잘못 입력한 값은 여기서 범위를 맞춘다.
	FLightShadowSettings GetShadowSettings() const
	{
		FLightShadowSettings Settings{};
		if (!LightComponent)
		{
			Settings.bCastShadows = false;
			return Settings;
		}
		Settings.Light = LightComponent;
		Settings.bCastShadows = LightComponent->CastsShadows();
		Settings.ResolutionScale = FMath::Clamp(LightComponent->GetShadowResolutionScale(), 0.0625f, 4.0f);
		Settings.Bias = std::max(LightComponent->GetShadowBias(), 0.0f);
		Settings.SlopeBias = std::max(LightComponent->GetShadowSlopeBias(), 0.0f);
		Settings.Sharpen = FMath::Clamp(LightComponent->GetShadowSharpen(), 0.0f, 1.0f);
		return Settings;
	}

	bool CastsDynamicShadow() const { return LightComponent && LightComponent->CastsShadows(); }
	float GetUserShadowBias() const { return LightComponent ? std::max(LightComponent->GetShadowBias(), 0.0f) : 0.0f; }
	float GetUserShadowSlopeBias() const { return LightComponent ? std::max(LightComponent->GetShadowSlopeBias(), 0.0f) : 0.0f; }
	float GetShadowResolutionScale() const { return LightComponent ? FMath::Clamp(LightComponent->GetShadowResolutionScale(), 0.0625f, 4.0f) : 1.0f; }
	float GetShadowSharpen() const { return LightComponent ? FMath::Clamp(LightComponent->GetShadowSharpen(), 0.0f, 1.0f) : 0.0f; }

	virtual bool IsDirectional() const { return false; }
	// 라이트가 영향을 주는 공간. 채널 배정, 뷰 컬링, Caster 판정에 쓴다.
	virtual FSphere GetBoundingSphere() const { return FSphere(FVector::ZeroVector, WORLD_MAX); }
	virtual bool AffectsBounds(const FSphere& Bounds) const
	{
		const FSphere LightBounds = GetBoundingSphere();
		const float Reach = LightBounds.Radius + Bounds.Radius;
		return (LightBounds.Center - Bounds.Center).SizeSquared() <= Reach * Reach;
	}
	// PCSS에 쓰는 광원의 겉보기 각도(도). Directional만 의미가 있다.
	virtual float GetLightSourceAngle() const { return 0.0f; }

	// 카메라와 무관한 그림자(Spot 1장, Point 6장)의 Initializer를 채운다.
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneView& View,
		TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const
	{
		return false;
	}

	// 카메라를 따라가는 그림자(Directional CSM)
	virtual uint32 GetNumViewDependentWholeSceneShadows(const FSceneView& View) const { return 0; }
	virtual bool GetViewDependentWholeSceneProjectedShadowInitializer(const FSceneView& View, int32 InCascadeIndex,
		FWholeSceneProjectedShadowInitializer& OutInitializer) const
	{
		return false;
	}
	virtual FSphere GetShadowSplitBounds(const FSceneView& View, int32 InCascadeIndex, FShadowCascadeSettings* OutCascadeSettings) const
	{
		return FSphere(FVector::ZeroVector, 0.0f);
	}

	FLightSceneInfo* GetLightSceneInfo() const { return LightSceneInfo; }

protected:
	const ULightComponent* LightComponent = nullptr;

private:
	friend class FScene;
	FScene* Scene = nullptr;
	int32 PackedIndex = INDEX_NONE;
	FLightSceneInfo* LightSceneInfo = nullptr;
};

class FDirectionalLightSceneProxy : public FLightSceneProxy
{
public:
	using FLightSceneProxy::FLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;

	bool IsDirectional() const override { return true; }
	float GetLightSourceAngle() const override;

	uint32 GetNumViewDependentWholeSceneShadows(const FSceneView& View) const override;
	bool GetViewDependentWholeSceneProjectedShadowInitializer(const FSceneView& View, int32 InCascadeIndex,
		FWholeSceneProjectedShadowInitializer& OutInitializer) const override;
	FSphere GetShadowSplitBounds(const FSceneView& View, int32 InCascadeIndex, FShadowCascadeSettings* OutCascadeSettings) const override;

	// 카메라에서 SplitIndex번째 캐스케이드 경계까지의 거리
	float GetSplitDistance(const FSceneView& View, uint32 SplitIndex) const;
};

class FLocalLightSceneProxy : public FLightSceneProxy
{
public:
	using FLightSceneProxy::FLightSceneProxy;
};

class FPointLightSceneProxy : public FLocalLightSceneProxy
{
public:
	using FLocalLightSceneProxy::FLocalLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;

	FSphere GetBoundingSphere() const override;
	bool GetWholeSceneProjectedShadowInitializer(const FSceneView& View,
		TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const override;
};

class FSpotLightSceneProxy : public FLocalLightSceneProxy
{
public:
	using FLocalLightSceneProxy::FLocalLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;

	FSphere GetBoundingSphere() const override;
	bool GetWholeSceneProjectedShadowInitializer(const FSceneView& View,
		TArray<FWholeSceneProjectedShadowInitializer>& OutInitializers) const override;
};

class FRectLightSceneProxy : public FLocalLightSceneProxy
{
public:
	using FLocalLightSceneProxy::FLocalLightSceneProxy;
	void GetLightShaderParameters(FLightRenderParameters& Out) const override;
};

class FAmbientLightSceneProxy
{
public:
	bool IsEnabled() const { return false; }
	bool IsShown(bool) const { return false; }
	const FAmbientLightRenderData& GetRenderData() const { return RenderData; }

private:
	FAmbientLightRenderData RenderData{};
};

class FSkyLightSceneProxy
{
public:
	bool IsEnabled() const { return false; }
	bool IsShown(bool) const { return false; }
	const FSkyLightRenderData& GetRenderData() const { return RenderData; }

private:
	FSkyLightRenderData RenderData{};
};
