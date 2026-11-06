#pragma once

#include "UI/Widget.h"
#include "TextBlock.generated.h"

class UFont;

UCLASS()
class UTextBlock : public UWidget
{
	GENERATED_BODY()


public:
	UTextBlock();

	void SetText(const FString& InText) { Text = InText; }
	const FString& GetText() const { return Text; }

	virtual FVector2 GetDesiredSize() const override;
	virtual void Paint(FUIDrawList& DrawList, float ParentOpacity) const override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual void OnLoaded() override { ResolveFont(); }

	UPROPERTY(EditAnywhere)
	FString Text = "Text Block";
	UPROPERTY(EditAnywhere)
	FString FontPath = "Assets/Fonts/Pretendard.json";
	UPROPERTY(EditAnywhere)
	float FontSize = 32.0f;                              // 디자인 해상도 기준 픽셀
	UPROPERTY(EditAnywhere)
	FLinearColor ColorAndOpacity = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	UPROPERTY(EditAnywhere)
	int32 Justification = 0;                             // 0 왼쪽, 1 가운데, 2 오른쪽

private:
	void ResolveFont();

	UFont* Font = nullptr;
};
