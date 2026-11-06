#pragma once

#include "Engine/ViewportClient.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Render/SceneView.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneViewTargets.h"
#include "UI/UIDrawList.h"

class UWorld;
class UUserWidget;
class UWidget;
class UGameInstance;
class UEngine;
class UCameraComponent;

struct FWorldContext;

// 게임 뷰포트 클라이언트 클래스
class FGameViewportClient : public FViewportClient
{
public:
	FGameViewportClient();
	virtual ~FGameViewportClient() override = default;

	// 월드 및 게임 인스턴스 초기화
	void Init(FWorldContext& InWorldContext, UEngine* InEngine);

	virtual UWorld* GetWorld() const override;

	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool InputKey(FViewport* Viewport, int32 Key, bool bDown) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) override;
	bool ConsumeExitRequest() {
		const bool bRequested = bExitRequested;
		bExitRequested = false;
		return bRequested;
	}

	bool ConsumeSIERequest() {
		const bool bRequested = bSIEModeRequested;
		bSIEModeRequested = false;
		return bRequested;
	}

	// 게임 카메라 시점 계산
	FSceneView CalcSceneView(const FRect& InViewRect);

	// 렌더 타깃 크기 관리
	virtual void Resize(FViewport* Viewport, uint32 NewWidth, uint32 NewHeight) override { Resize(NewWidth, NewHeight); }
	void Resize(uint32 InWidth, uint32 InHeight);

	const FRect& GetRect() const { return Rect; }
	void SetRect(const FRect& InRect) { Rect = InRect; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	class FTexture2D* GetColorTarget() const { return ViewTargets.SceneColor.get(); }
	FTexture2D* SwapSceneColorAndBind(TUniquePtr<FTexture2D>& Output) { return ViewTargets.SwapSceneColorAndBind(Output); }
	class FTexture2D* GetDepthTarget() const { return ViewTargets.Depth.get(); }
	FSceneRenderData& GetRenderData() { return RenderData; }
	FSceneViewTargets& GetViewTargets() { return ViewTargets; }
	const FSceneViewTargets& GetViewTargets() const { return ViewTargets; }

	void SetGameInstance(UGameInstance* InGameInstance) { GameInstance = InGameInstance; }
	UGameInstance* GetGameInstance() const { return GameInstance; }

	void SetWorld(UWorld* InWorld);

	void SetCameraComponent(UCameraComponent* InCameraComponent);
	// 이 뷰포트가 실제로 그리고 마우스 Ray를 만드는 카메라. 첫 CalcSceneView 전에는 null일 수 있다.
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	void Reset();

	bool BeginSIEMode();
	void EndSIEMode();
	void GetKeyInputBySIEMode(int32 Key, bool bDown);
	void GetAxisInputBySIEMode(EGameInputAxis Key, float Delta);
	void TickSIEInput(float DeltaTime, float MoveSpeed, float MouseSensitivity);
	bool IsSIEMode() const { return bSIEMode; }

	virtual void LostFocus() override;

	// 화면에 올라간 UI
	void AddViewportWidget(UUserWidget* Widget);
	void RemoveViewportWidget(UUserWidget* Widget);
	void ClearViewportWidgets();
	void TickUI(float DeltaTime);
	// 화면 크기로 배치하고 DrawList를 채운다 (GameUI 오버레이 패스에서 호출)
	void PaintUI(FUIDrawList& DrawList);

	static constexpr float UIDesignHeight = 1080.0f;

	bool ScreenToWorldRay(
		float X, float Y, FRay* OutRay);

protected:
	virtual bool HandleUIKey(int32 Key, bool bDown);
	virtual bool HandleUIAxis(EGameInputAxis AxisKey, float Delta);
	virtual bool HandleUIMouseMove(int32 X, int32 Y);

private:
	UCameraComponent* FindPlayerCamera() const;
	UWidget* HitTestUI(float X, float Y) const;

	TArray<UUserWidget*> ViewportWidgets;   // ZOrder 오름차순
	UWidget* HoveredWidget = nullptr;
	UWidget* PressedWidget = nullptr;
	int32 UIMouseX = 0;
	int32 UIMouseY = 0;

	UWorld* World = nullptr;
	UGameInstance* GameInstance = nullptr;
	UEngine* Engine = nullptr;
	UCameraComponent* CameraComponent = nullptr;
	bool bSIEMode = false;
	bool bPreviousMainCameraExternalInputManaged = false;
	bool bSIEKeyDown[256]{};
	float SIEMouseDeltaX = 0.0f;
	float SIEMouseDeltaY = 0.0f;
	float SIEWheelDelta = 0.0f;
	bool bExitRequested = false;
	bool bSIEModeRequested = false;

	// 렌더 타깃 버퍼
	FRect Rect{};
	uint32 Width = 0;
	uint32 Height = 0;
	FSceneViewTargets ViewTargets;
	FSceneRenderData RenderData;
};
