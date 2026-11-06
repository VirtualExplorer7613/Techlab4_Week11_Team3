#pragma once

#include "UI/Widget.h"
#include "PanelWidget.generated.h"

UCLASS()
class UPanelWidget : public UWidget
{
	GENERATED_BODY()

public:
	UPanelWidget() = default;
	virtual ~UPanelWidget() override;          // 자식을 함께 삭제한다

	// 다른 부모에 붙어 있었다면 떼어 와서 붙인다.
	UWidget* AddChild(UWidget* Child);
	// 떼기만 한다. 삭제는 호출한 쪽이 한다.
	bool RemoveChild(UWidget* Child);

	int32 GetChildCount() const { return Children.Num(); }
	const TArray<UWidget*>& GetChildren() const { return Children; }
	// ZOrder 오름차순 (같으면 추가 순서). 그리기와 히트 테스트가 같은 순서를 쓴다.
	void GetSortedChildren(TArray<UWidget*>& OutChildren) const;

	// 자손 중 이름이 같은 첫 위젯
	UWidget* FindWidget(const FString& Name) const;

	virtual void SetGeometry(const FUIRect& InRect, float InScale) override;
	virtual void Paint(FUIDrawList& DrawList, float ParentOpacity) const override;
	virtual void Tick(float DeltaTime) override;
	virtual UWidget* HitTest(float X, float Y) override;

protected:
	TArray<UWidget*> Children;
};

// 자식을 앵커로 자유 배치한다. 루트로 가장 많이 쓴다.
UCLASS()
class UCanvasPanel : public UPanelWidget
{
	GENERATED_BODY()

public:
	UCanvasPanel()
	{
		Anchors = FVector4(0.0f, 0.0f, 1.0f, 1.0f);   // 기본값: 부모를 꽉 채움
		Offsets = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
};

// 자식을 위에서 아래로 쌓는다. 순위표처럼 줄이 늘어나는 점수판에 쓴다.
// 자식의 앵커는 무시하고, 높이는 자식의 GetDesiredSize().Y를 쓴다.
UCLASS()
class UVerticalBox : public UPanelWidget
{
	GENERATED_BODY()


public:
	virtual void SetGeometry(const FUIRect& InRect, float InScale) override;
	virtual FVector2 GetDesiredSize() const override;

	UPROPERTY(EditAnywhere)
	float Spacing = 4.0f;
};
