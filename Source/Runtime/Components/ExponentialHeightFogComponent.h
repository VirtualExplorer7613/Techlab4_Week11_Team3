#pragma once
#include "SceneComponent.h"
#include "Math/Vector4.h"
#include "ExponentialHeightFogComponent.generated.h"

UCLASS()
class UExponentialHeightFogComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	UExponentialHeightFogComponent() = default;
	~UExponentialHeightFogComponent() override = default;

	float GetFogDensity() const { return FogDensity; }
	void SetFogDensity(float InDensity) { FogDensity = InDensity; }

	float GetFogHeightFalloff() const { return FogHeightFalloff; }
	void SetFogHeightFalloff(float InFalloff) { FogHeightFalloff = InFalloff; }

	const FVector4& GetFogColor() const { return FogColor; }
	void SetFogColor(const FVector4& InColor) { FogColor = InColor; }

	float GetStartDistance() const { return StartDistance; }
	void SetStartDistance(float InDistance) { StartDistance = InDistance; }

	float GetFogMaxOpacity() const { return FogMaxOpacity; }
	void SetFogMaxOpacity(float InOpacity) { FogMaxOpacity = InOpacity; }

	float GetFogCutoffDistance() const { return FogCutoffDistance; }
	void SetFogCutoffDistance(float InDistance) { FogCutoffDistance = InDistance; }

	float GetFogHeight() const { return GetWorldLocation().Z; }

private:
	UPROPERTY(EditAnywhere)
	float FogDensity = 0.02f;
	UPROPERTY(EditAnywhere)
	float FogHeightFalloff = 0.02f;
	UPROPERTY(EditAnywhere, Type=Color)
	FVector4 FogColor = FVector4(0.5f, 0.6f, 0.7f, 1.0f);
	UPROPERTY(EditAnywhere)
	float StartDistance = 0.0f;
	UPROPERTY(EditAnywhere)
	float FogMaxOpacity = 1.0f;
	UPROPERTY(EditAnywhere)
	float FogCutoffDistance = 0.0f;
};