#include "EnginePCH.h"
#include "UI/UserWidget.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

UUserWidget::UUserWidget()
{
	Anchors = FVector4(0.0f, 0.0f, 1.0f, 1.0f);   // 화면 전체
	Offsets = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
}

UUserWidget::~UUserWidget()
{
	// Destroy()를 거치지 않고 지워지는 경우(엔진 종료 시 일괄 삭제)에도 뷰포트 목록에 남지 않게 한다.
	// OnDestroyed는 부르지 않는다. 콜백을 건 액터가 이미 지워졌을 수 있다.
	RemoveFromViewport();
	SetRootWidget(nullptr);
}

void UUserWidget::SetRootWidget(UPanelWidget* InRoot)
{
	if (RootWidget == InRoot) return;
	if (RootWidget)
	{
		RootWidget->RootOwner = nullptr;   // 루트의 소멸자가 이쪽을 다시 건드리지 않게 한다
		delete RootWidget;
	}
	RootWidget = InRoot;
	if (RootWidget)
		RootWidget->RootOwner = this;
}

UWidget* UUserWidget::FindWidget(const FString& Name) const
{
	if (!RootWidget) return nullptr;
	if (RootWidget->WidgetName == Name) return RootWidget;
	return RootWidget->FindWidget(Name);
}

void UUserWidget::AddToViewport(int32 InZOrder)
{
	if (OwningViewport) return;

	FWorldContext* Context = GEngine ? GEngine->GetWorldContextFromWorld(OwningWorld) : nullptr;
	if (!Context || !Context->GameViewport)
	{
		HTR_LOG(Warning, "AddToViewport: no game viewport for widget {}", GetName());
		return;
	}

	ZOrder = InZOrder;
	Context->GameViewport->AddViewportWidget(this);

	if (!bConstructed)
	{
		bConstructed = true;
		NativeConstruct();
	}
}

void UUserWidget::RemoveFromViewport()
{
	if (OwningViewport)
		OwningViewport->RemoveViewportWidget(this);
}

void UUserWidget::Destroy()
{
	RemoveFromViewport();
	if (bConstructed)
		NativeDestruct();
	if (OnDestroyed)
		OnDestroyed();
	delete this;
}

void UUserWidget::SetGeometry(const FUIRect& InRect, float InScale)
{
	Super::SetGeometry(InRect, InScale);
	if (RootWidget)
		RootWidget->Arrange({ CachedRect, InScale });
}

void UUserWidget::Paint(FUIDrawList& DrawList, float ParentOpacity) const
{
	if (IsVisible() && RootWidget)
		RootWidget->Paint(DrawList, ParentOpacity * RenderOpacity);
}

void UUserWidget::Tick(float DeltaTime)
{
	NativeTick(DeltaTime);
	if (RootWidget)
		RootWidget->Tick(DeltaTime);
}

UWidget* UUserWidget::HitTest(float X, float Y)
{
	return (IsHitTestable() && RootWidget) ? RootWidget->HitTest(X, Y) : nullptr;
}