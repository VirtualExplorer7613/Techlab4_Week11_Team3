#include "EnginePCH.h"
#include "LightSceneInfo.h"

#include "Engine/LightSceneProxy.h"

bool FLightSceneInfo::IsDirectional() const
{
	return Proxy && Proxy->IsDirectional();
}

FSphere FLightSceneInfo::GetBoundingSphere() const
{
	return Proxy ? Proxy->GetBoundingSphere() : FSphere(FVector::ZeroVector, 0.0f);
}

bool FLightSceneInfo::AffectsBounds(const FSphere& Bounds) const
{
	return Proxy && Proxy->AffectsBounds(Bounds);
}
