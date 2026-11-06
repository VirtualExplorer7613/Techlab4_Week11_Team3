#pragma once

#include "Core/Types.h"
#include "Math/Sphere.h"

class FLightSceneProxy;
class FScene;

// 라이트의 렌더러 쪽 상태. Proxy는 컴포넌트를 읽는 창구이고, 이 구조체는 렌더러가 관리한다.
struct FLightSceneInfo
{
	// FScene::Lights 안의 고정 인덱스. 다른 라이트가 지워져도 바뀌지 않는다.
	int32 Id = INDEX_NONE;
	FLightSceneProxy* Proxy = nullptr;
	FScene* Scene = nullptr;

	bool IsDirectional() const;
	// 라이트가 영향을 주는 공간. Directional은 무한대 반지름.
	FSphere GetBoundingSphere() const;
	// Bounds 안의 물체가 이 라이트의 영향을 받을 수 있는지
	bool AffectsBounds(const FSphere& Bounds) const;

	void SetDynamicShadowMapChannel(int32 NewChannel) { DynamicShadowMapChannel = NewChannel; }
	int32 GetDynamicShadowMapChannel() const { return DynamicShadowMapChannel; }

protected:
	// Forward ShadowMask에서 이 라이트가 쓰는 채널. 그림자가 없으면 INDEX_NONE.
	int32 DynamicShadowMapChannel = INDEX_NONE;
};
