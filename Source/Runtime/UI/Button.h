#pragma once

#include "UI/PanelWidget.h"

#include <functional>
#include "Button.generated.h"

// 자식(보통 UTextBlock 라벨)을 가지는 버튼. 버튼 전체가 하나의 클릭 대상이다.
UCLASS()
class UButton : public UPanelWidget
{
	GENERATED_BODY()


public:
	std::function<void()> OnClicked;

	virtual void Paint(FUIDrawList& DrawList, float ParentOpacity) const override;
	// 라벨이 클릭을 가져가지 않도록 패널의 히트 테스트 대신 자기 영역만 본다.
	virtual UWidget* HitTest(float X, float Y) override { return UWidget::HitTest(X, Y); }

	virtual void OnMouseEnter() override { bHovered = true; }
	virtual void OnMouseLeave() override { bHovered = false; }
	virtual bool OnMouseButtonDown() override { bPressed = true; return true; }
	virtual void OnMouseButtonUp(bool bInside) override
	{
		bPressed = false;
		if (bInside && OnClicked) OnClicked();
	}

	UPROPERTY(EditAnywhere)
	FLinearColor NormalColor = FLinearColor(0.12f, 0.15f, 0.20f, 0.90f);
	UPROPERTY(EditAnywhere)
	FLinearColor HoveredColor = FLinearColor(0.20f, 0.25f, 0.32f, 0.95f);
	UPROPERTY(EditAnywhere)
	FLinearColor PressedColor = FLinearColor(0.85f, 0.55f, 0.10f, 1.00f);

private:
	bool bHovered = false;
	bool bPressed = false;
};
