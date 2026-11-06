#include "EnginePCH.h"
#include "UI/PanelWidget.h"

#include <algorithm>

UPanelWidget::~UPanelWidget()
{
	// 자식의 소멸자가 RemoveChild로 배열을 건드리지 않도록 부모 연결을 먼저 끊고 지운다.
	const TArray<UWidget*> Owned = Children;
	Children.Reset();
	for (UWidget* Child : Owned)
	{
		Child->Parent = nullptr;
		delete Child;
	}
}

UWidget* UPanelWidget::AddChild(UWidget* Child)
{
	if (!Child || Child == this) return nullptr;
	Child->RemoveFromParent();
	Child->Parent = this;
	Children.Add(Child);
	return Child;
}

bool UPanelWidget::RemoveChild(UWidget* Child)
{
	const int32 Index = Children.Find(Child);
	if (Index == INDEX_NONE) return false;
	Children.RemoveAt(Index);
	Child->Parent = nullptr;
	return true;
}

void UPanelWidget::GetSortedChildren(TArray<UWidget*>& OutChildren) const
{
	OutChildren = Children;
	std::stable_sort(OutChildren.begin(), OutChildren.end(),
		[](const UWidget* A, const UWidget* B) { return A->ZOrder < B->ZOrder; });
}

UWidget* UPanelWidget::FindWidget(const FString& Name) const
{
	for (UWidget* Child : Children)
	{
		if (Child->WidgetName == Name) return Child;
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Child))
			if (UWidget* Found = Panel->FindWidget(Name)) return Found;
	}
	return nullptr;
}

void UPanelWidget::SetGeometry(const FUIRect& InRect, float InScale)
{
	Super::SetGeometry(InRect, InScale);
	const FUIGeometry Mine{ CachedRect, InScale };
	for (UWidget* Child : Children)
		if (!Child->IsCollapsed())
			Child->Arrange(Mine);
}

void UPanelWidget::Paint(FUIDrawList& DrawList, float ParentOpacity) const
{
	if (!IsVisible()) return;
	TArray<UWidget*> Sorted;
	GetSortedChildren(Sorted);
	const float Opacity = ParentOpacity * RenderOpacity;
	for (const UWidget* Child : Sorted)
		Child->Paint(DrawList, Opacity);
}

void UPanelWidget::Tick(float DeltaTime)
{
	for (UWidget* Child : Children)
		Child->Tick(DeltaTime);
}

UWidget* UPanelWidget::HitTest(float X, float Y)
{
	// HitTestInvisible 패널은 자식까지 모두 통과시킨다.
	if (!IsHitTestable() || !CachedRect.Contains(X, Y)) return nullptr;

	TArray<UWidget*> Sorted;
	GetSortedChildren(Sorted);
	for (auto It = Sorted.rbegin(); It != Sorted.rend(); ++It)   // 위에 그린 것부터
		if (UWidget* Hit = (*It)->HitTest(X, Y))
			return Hit;
	return nullptr;   // 패널 자신은 입력을 받지 않는다
}

void UVerticalBox::SetGeometry(const FUIRect& InRect, float InScale)
{
	UWidget::SetGeometry(InRect, InScale);   // 캔버스식 앵커 배치는 건너뛴다

	float Y = CachedRect.Y;
	for (UWidget* Child : Children)
	{
		if (Child->IsCollapsed()) continue;
		const float H = Child->GetDesiredSize().Y * InScale;
		Child->SetGeometry(FUIRect{ CachedRect.X, Y, CachedRect.W, H }, InScale);
		Y += H + Spacing * InScale;
	}
}

FVector2 UVerticalBox::GetDesiredSize() const
{
	FVector2 Size(0.0f, 0.0f);
	int32 Count = 0;
	for (const UWidget* Child : Children)
	{
		if (Child->IsCollapsed()) continue;
		const FVector2 ChildSize = Child->GetDesiredSize();
		Size.X = std::max(Size.X, ChildSize.X);
		Size.Y += ChildSize.Y;
		++Count;
	}
	if (Count > 1) Size.Y += Spacing * (Count - 1);
	return Size;
}