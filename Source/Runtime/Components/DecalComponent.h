#pragma once

#include "SceneComponent.h"
#include "DecalComponent.generated.h"

class FLineBatcher;
class UTexture2D;

UCLASS()
class UDecalComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UDecalComponent();
	virtual ~UDecalComponent() override = default;

	const FVector GetDecalSize() const { return DecalSize; }
	void SetDecalSize(const FVector& InSize) { DecalSize = InSize; }

	virtual FBox CalcLocalBounds() const override;

	void DrawDebug(FLineBatcher* LineBatcher) const;

	UTexture2D* GetDecalTexture() const { return DecalTexture; }
	FMatrix GetWorldToDecal() const;

	virtual void OnRegister() override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	FOBB GetDecalOBB() const;

	virtual void TickComponent(float DeltaTime) override;

	float GetFadeAlpha() const;

	bool IsAdditiveBlend() const { return bAdditiveBlend; }
	void SetAdditiveBlend(bool bInAdditive) { bAdditiveBlend = bInAdditive; }

	FColor GetDecalColor() const { return DecalColor; }
	void SetDecalColor(const FColor& InColor) { DecalColor = InColor; }
	float GetIntensity() const { return Intensity; }
	void SetIntensity(float InIntensity) { Intensity = InIntensity; }

	void SetDecalTexturePath(const FString& InPath) { DecalTexturePath = InPath; }

private:
	void ResolveTexture();

	UPROPERTY(EditAnywhere)
	FVector DecalSize = FVector(1.f, 1.f, 1.f);
	UPROPERTY(EditAnywhere)
	FString DecalTexturePath = "Assets/Checker.png";
	UPROPERTY(EditAnywhere)
	float FadeInStartDelay = 0.f;
	UPROPERTY(EditAnywhere)
	float FadeInDuration = 0.f;   
	UPROPERTY(EditAnywhere)
	float FadeStartDelay = 0.f;  
	UPROPERTY(EditAnywhere)
	float FadeDuration = 0.f;      
	UPROPERTY(EditAnywhere)
	bool bAdditiveBlend = false;
	UPROPERTY(EditAnywhere)
	FColor DecalColor = FColor(255, 255, 255, 255);
	UPROPERTY(EditAnywhere)
	float Intensity = 1.f;


	float ElapsedTime = 0.f;       

	UTexture2D* DecalTexture = nullptr;
};