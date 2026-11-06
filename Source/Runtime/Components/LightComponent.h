#pragma once

#include "LightComponentBase.h"
#include "Render/LineBatcher.h"
#include "LightComponent.generated.h"

UCLASS()
class ULightComponent : public ULightComponentBase
{
	GENERATED_BODY()
public:
	ULightComponent();
	virtual ~ULightComponent() override;

	float GetShadowResolutionScale() const { return ShadowResolutionScale; }
	float GetShadowBias() const { return ShadowBias; }
	float GetShadowSlopeBias() const { return ShadowSlopeBias; }
	float GetShadowSharpen() const { return ShadowSharpen; }

	void SetShadowResolutionScale(float InScale) { ShadowResolutionScale = InScale; }
	void SetShadowBias(float InBias) { ShadowBias = InBias; }
	void SetShadowSlopeBias(float InSlopeBias) { ShadowSlopeBias = InSlopeBias; }
	void SetShadowSharpen(float InSharpen) { ShadowSharpen = InSharpen; }

	virtual ELightType GetLightType() const = 0;
	virtual void DrawDebug(FLineBatcher* LineBatcher) const;

	virtual class FLightSceneProxy* CreateSceneProxy() const
	{
		return NULL;
	}
	FLightSceneProxy* SceneProxy = nullptr;
protected:
	virtual bool ShouldCreateRenderState() const override
	{ return true; }

	virtual void CreateRenderState() override;
	virtual void DestroyRenderState() override;

	// 그림자 해상도 배율 (언리얼 ShadowResolutionScale). 화면 크기로 정한 해상도에 곱한 뒤 2의 거듭제곱으로 내린다.
	UPROPERTY(EditAnywhere)
	float ShadowResolutionScale = 1.0f;
	// 깊이 bias 배율 (언리얼 ShadowBias). r.Shadow.*DepthBias 기본값에 곱한다. 0.5가 기본, 키우면 Shadow Acne가 줄고 Peter Panning이 는다.
	UPROPERTY(EditAnywhere)
	float ShadowBias = 0.5f;
	// 기울기 bias 배율 (언리얼 ShadowSlopeBias). 빛과 비스듬한 면일수록 깊이를 더 민다. 0.5가 기본.
	UPROPERTY(EditAnywhere)
	float ShadowSlopeBias = 0.5f;
	// 0이면 PCF 결과 그대로, 1에 가까울수록 반그림자 폭을 좁혀 가장자리를 날카롭게 만든다.
	UPROPERTY(EditAnywhere)
	float ShadowSharpen = 0.0f;
private:


};