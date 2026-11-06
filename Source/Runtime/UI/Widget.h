#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UI/UIDrawList.h"
#include "Widget.generated.h"

class UPanelWidget;
class UUserWidget;

enum class EUIVisibility : int32
{
	Visible = 0,           // 보이고 클릭 가능
	Hidden = 1,            // 자리는 차지하고 안 보임
	Collapsed = 2,         // 자리도 차지하지 않음
	HitTestInvisible = 3,  // 보이지만 클릭은 통과 (점수판 같은 HUD에 사용)
};

// 부모가 자식에게 넘기는 영역과 해상도 배율
struct FUIGeometry
{
	FUIRect Rect;
	float Scale = 1.0f;   // 화면 높이 / 1080
};

UCLASS()
class UWidget : public UObject
{
	GENERATED_BODY()


public:
	UWidget() = default;
	// 엔진 종료 시 UObject 일괄 삭제는 순서가 정해져 있지 않다.
	// 먼저 지워지는 위젯이 부모·루트 소유자에서 스스로 빠져 이중 삭제를 막는다.
	virtual ~UWidget() override;

	// 부모 영역과 앵커로 자기 영역을 계산한 뒤 SetGeometry를 부른다.
	void Arrange(const FUIGeometry& ParentGeometry);
	// 계산된 영역을 저장한다. 패널은 여기서 자식을 배치한다.
	virtual void SetGeometry(const FUIRect& InRect, float InScale);

	virtual void Paint(FUIDrawList& DrawList, float ParentOpacity) const {}
	virtual void Tick(float DeltaTime) {}

	// 디자인 해상도 기준 기본 크기. bAutoSize면 Offsets 대신 이 값을 쓴다.
	virtual FVector2 GetDesiredSize() const { return FVector2(Offsets.Z, Offsets.W); }

	// 화면 좌표 (X, Y)에서 입력을 받을 위젯
	virtual UWidget* HitTest(float X, float Y);

	virtual void OnMouseEnter() {}
	virtual void OnMouseLeave() {}
	// true를 돌려주면 이 위젯이 버튼을 잡는다 (뗄 때 OnMouseButtonUp이 온다).
	virtual bool OnMouseButtonDown() { return false; }
	virtual void OnMouseButtonUp(bool bInside) {}

	// JSON에서 속성을 읽은 직후 (2단계). 경로 문자열로 폰트·텍스처를 다시 찾는 데 쓴다.
	virtual void OnLoaded() {}

	void RemoveFromParent();

	bool IsVisible() const
	{
		return Visibility == static_cast<int32>(EUIVisibility::Visible)
			|| Visibility == static_cast<int32>(EUIVisibility::HitTestInvisible);
	}
	bool IsHitTestable() const { return Visibility == static_cast<int32>(EUIVisibility::Visible); }
	bool IsCollapsed() const { return Visibility == static_cast<int32>(EUIVisibility::Collapsed); }

	const FUIRect& GetCachedRect() const { return CachedRect; }
	float GetCachedScale() const { return CachedScale; }
	UPanelWidget* GetParent() const { return Parent; }

	UPROPERTY(EditAnywhere)
	FString WidgetName;
	UPROPERTY(EditAnywhere)
	FVector4 Anchors = FVector4(0.0f, 0.0f, 0.0f, 0.0f);     // MinX, MinY, MaxX, MaxY (0~1)
	UPROPERTY(EditAnywhere)
	FVector4 Offsets = FVector4(0.0f, 0.0f, 100.0f, 30.0f);  // 점: X, Y, 폭, 높이 / 늘어남: 왼, 위, 오른, 아래 여백
	UPROPERTY(EditAnywhere)
	float AlignmentX = 0.0f;
	UPROPERTY(EditAnywhere)
	float AlignmentY = 0.0f;
	UPROPERTY(EditAnywhere)
	bool bAutoSize = false;
	UPROPERTY(EditAnywhere)
	int32 ZOrder = 0;
	UPROPERTY(EditAnywhere)
	int32 Visibility = 0;                                   // EUIVisibility
	UPROPERTY(EditAnywhere)
	float RenderOpacity = 1.0f;

protected:
	friend class UPanelWidget;
	friend class UUserWidget;

	UPanelWidget* Parent = nullptr;
	UUserWidget* RootOwner = nullptr;   // 이 위젯을 루트로 가진 UUserWidget
	FUIRect CachedRect;
	float CachedScale = 1.0f;
};