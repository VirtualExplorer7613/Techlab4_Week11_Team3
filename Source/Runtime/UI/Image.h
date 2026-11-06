#pragma once

#include "UI/Widget.h"
#include "Image.generated.h"

class UTexture2D;

// 텍스처가 없으면 단색 판으로 그린다 (점수판 배경에 사용).
UCLASS()
class UImage : public UWidget
{
	GENERATED_BODY()


public:
	virtual void Paint(FUIDrawList& DrawList, float ParentOpacity) const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual void OnLoaded() override { ResolveTexture(); }

	UPROPERTY(EditAnywhere)
	FString TexturePath;
	UPROPERTY(EditAnywhere)
	FLinearColor ColorAndOpacity = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

private:
	void ResolveTexture();

	UTexture2D* Texture = nullptr;
};
