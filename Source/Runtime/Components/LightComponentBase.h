#pragma once

#include "SceneComponent.h"
#include "Math/Color.h"
#include "LightComponentBase.generated.h"

enum class ELightType { Directional, Point, Spot, Rect, Fireball, Sky };

UCLASS()
class ULightComponentBase : public USceneComponent
{
	GENERATED_BODY()
public:
	ULightComponentBase();
	virtual ~ULightComponentBase() override;


	// Getter
	float GetIntensity() const { return Intensity; }
	FColor GetLightColor() const { return LightColor; }
	bool CastsShadows() const { return bCastShadows; }

	// Setter
	void SetIntensity(float InIntensity) { Intensity = InIntensity; }
	void SetLightColor(FColor InLightColor) { LightColor = InLightColor; }
	void SetCastShadows(bool bInCastShadows) { bCastShadows = bInCastShadows; }


protected:
	UPROPERTY(EditAnywhere)
	float Intensity = 1.0f;
	UPROPERTY(EditAnywhere)
	FColor LightColor = FColor(255, 255, 255, 255);
	UPROPERTY(EditAnywhere)
	bool bCastShadows = true;

private:

};
