#pragma once

#include "UI/PanelWidget.h"

#include <functional>
#include "UserWidget.generated.h"

class UWorld;
class FGameViewportClient;

UCLASS()
class UUserWidget : public UWidget
{
	GENERATED_BODY()

public:
	UUserWidget();
	virtual ~UUserWidget() override;

	void SetRootWidget(UPanelWidget* InRoot);   // 이전 루트는 삭제한다
	UPanelWidget* GetRootWidget() const { return RootWidget; }

	UWidget* FindWidget(const FString& Name) const;
	template <typename T>
	T* GetWidget(const FString& Name) const { return Cast<T>(FindWidget(Name)); }

	void AddToViewport(int32 InZOrder = 0);
	void RemoveFromViewport();
	bool IsInViewport() const { return OwningViewport != nullptr; }
	// 화면에서 떼고 삭제한다. 이후 포인터를 쓰면 안 된다.
	void Destroy();
	// Destroy 직전에 불린다. 위젯 포인터를 들고 있는 쪽이 nullptr로 비우는 데 쓴다.
	std::function<void()> OnDestroyed;

	void SetOwningWorld(UWorld* InWorld) { OwningWorld = InWorld; }
	UWorld* GetOwningWorld() const { return OwningWorld; }

	virtual void SetGeometry(const FUIRect& InRect, float InScale) override;
	virtual void Paint(FUIDrawList& DrawList, float ParentOpacity) const override;
	virtual void Tick(float DeltaTime) override;
	virtual UWidget* HitTest(float X, float Y) override;

	FString AssetPath;   // 2단계: 어떤 .widget에서 만들어졌는지

protected:
	// 처음 AddToViewport될 때 한 번. 여기서 GetWidget으로 멤버를 묶는다.
	virtual void NativeConstruct() {}
	virtual void NativeDestruct() {}
	virtual void NativeTick(float DeltaTime) {}

private:
	friend class FGameViewportClient;
	friend class UWidget;   // 루트가 먼저 지워지면 RootWidget을 비운다

	UPanelWidget* RootWidget = nullptr;
	UWorld* OwningWorld = nullptr;
	FGameViewportClient* OwningViewport = nullptr;
	bool bConstructed = false;
};

// .widget 에셋에서 위젯 트리를 만든다. 클래스는 에셋의 ParentClass를 따른다 (WidgetSerializer.cpp에 구현).
UUserWidget* CreateWidget(UWorld* World, const FString& AssetPath);
