#include "EnginePCH.h"
#include "DirectionalLightComponent.h"

#include "Engine/LightSceneProxy.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{

}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

void UDirectionalLightComponent::SetAngularRadiusDegrees(float InDegrees)
{
    AngularRadiusDegrees = FMath::Clamp(InDegrees, 0.0f, 10.0f);
}

void UDirectionalLightComponent::SetDynamicShadowDistanceMovableLight(float NewValue)
{
    DynamicShadowDistanceMovableLight = std::max(NewValue, 0.0f);
}

void UDirectionalLightComponent::SetDynamicShadowCascades(uint32 NewValue)
{
    DynamicShadowCascades = std::clamp(NewValue, 1u, MaxDynamicShadowCascades);
}

void UDirectionalLightComponent::SetCascadeDistributionExponent(float NewValue)
{
    CascadeDistributionExponent = FMath::Clamp(NewValue, 1.0f, 4.0f);
}

void UDirectionalLightComponent::SetCascadeTransitionFraction(float NewValue)
{
    CascadeTransitionFraction = FMath::Clamp(NewValue, 0.0f, 0.3f);
}

void UDirectionalLightComponent::SetShadowDistanceFadeoutFraction(float NewValue)
{
    ShadowDistanceFadeoutFraction = FMath::Clamp(NewValue, 0.0f, 1.0f);
}

void UDirectionalLightComponent::PostEditChangeProperty(const FProperty& Property)
{
    if (Property.Name == "AngularRadiusDegrees")
        SetAngularRadiusDegrees(AngularRadiusDegrees);
    else if (Property.Name == "DynamicShadowDistanceMovableLight")
        SetDynamicShadowDistanceMovableLight(DynamicShadowDistanceMovableLight);
    else if (Property.Name == "DynamicShadowCascades")
        SetDynamicShadowCascades(DynamicShadowCascades);
    else if (Property.Name == "CascadeDistributionExponent")
        SetCascadeDistributionExponent(CascadeDistributionExponent);
    else if (Property.Name == "CascadeTransitionFraction")
        SetCascadeTransitionFraction(CascadeTransitionFraction);
    else if (Property.Name == "ShadowDistanceFadeoutFraction")
        SetShadowDistanceFadeoutFraction(ShadowDistanceFadeoutFraction);
    Super::PostEditChangeProperty(Property);
}

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const
{
    return new FDirectionalLightSceneProxy(this);
}
