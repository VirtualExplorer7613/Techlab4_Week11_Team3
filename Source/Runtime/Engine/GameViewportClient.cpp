#include "EnginePCH.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/ViewportCameraMovement.h"
#include <cmath>
#include "Math/Frustum.h"
#include "Render/RenderCommand.h"
#include "UI/UserWidget.h"

FGameViewportClient::FGameViewportClient()
{
}

void FGameViewportClient::Init(FWorldContext& InWorldContext, UEngine* InEngine)
{
	SetWorld(InWorldContext.World());
	GameInstance = InWorldContext.OwningGameInstance;
	Engine = InEngine;
}

UWorld* FGameViewportClient::GetWorld() const
{
	return World;
}

void FGameViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	HandleUIMouseMove(X, Y);
}

bool FGameViewportClient::InputKey(FViewport* Viewport, int32 Key, bool bDown)
{
	if (HandleUIKey(Key, bDown))
		return true;
	if (bDown && Key == static_cast<int32>(EKeyCode::Escape))
	{
		bExitRequested = true;
		return true;
	}

	if (bDown && Key == static_cast<int32>(EKeyCode::F8))
	{
		bSIEModeRequested = true;
		return true;
	}

	if (bSIEMode && Key == static_cast<int32>(EKeyCode::LButton)) return true;

	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
		{
			PC->InputKey(Key, bDown);
			return true;
		}
	}
	return false;
}

bool FGameViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime)
{
	if (HandleUIAxis(AxisKey, Delta))
		return true;

	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
		{
			PC->InputAxis(AxisKey, Delta);
			return true;
		}
	}
	return false;
}

void FGameViewportClient::LostFocus()
{
	std::fill(std::begin(bSIEKeyDown), std::end(bSIEKeyDown), false);
	SIEMouseDeltaX = SIEMouseDeltaY = SIEWheelDelta = 0.0f;
	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
			PC->ClearInputState();
	}
}

bool FGameViewportClient::ScreenToWorldRay(
	float X, float Y, FRay* OutRay)
{
	assert(OutRay);

	const FRect& Rect = GetRect();

	if (!CameraComponent ||
		Rect.Width <= 0.0f || Rect.Height <= 0.0f ||
		Width == 0 || Height == 0)
	{
		return false;
	}

	if (X < 0.0f || Y < 0.0f ||
		X >= Rect.Width || Y >= Rect.Height)
	{
		return false;
	}

	const float RenderX = X * Width / Rect.Width;
	const float RenderY = Y * Height / Rect.Height;

	*OutRay = CameraComponent->DeProjection(
		FVector2(RenderX, RenderY),
		static_cast<float>(Width),
		static_cast<float>(Height));

	return true;
}

UCameraComponent* FGameViewportClient::FindPlayerCamera() const
{
	if (!World) return nullptr;
	if (APawn* Pawn = World->GetPlayerPawn())
	{
		if (ADefaultPawn* DefaultPawn = Cast<ADefaultPawn>(Pawn))
			if (UCameraComponent* Camera = DefaultPawn->GetCameraComponent()) return Camera;
		for (UActorComponent* Component : Pawn->GetComponents())
			if (UCameraComponent* Camera = Cast<UCameraComponent>(Component)) return Camera;
	}
	return nullptr;
}

void FGameViewportClient::SetWorld(UWorld* InWorld)
{
	if (World == InWorld) return;
	EndSIEMode();
	LostFocus();
	RenderData.ResetScene();
	CameraComponent = nullptr;
	bExitRequested = bSIEModeRequested = false;
	World = InWorld;
}

namespace
{
	bool SetCameraWorldPose(UCameraComponent& Camera, const FVector& Location, const FRotator& Rotation)
	{
		FTransform Relative = Camera.GetTransform();
		if (const USceneComponent* Parent = Camera.GetAttachParent())
		{
			const FMatrix ParentWorld = Parent->GetWorldMatrix();
			const float Determinant = ParentWorld.Determinant();
			if (!std::isfinite(Determinant) || Determinant == 0.0f)
				return false;
			Relative.Location = ParentWorld.Inverse().TransformPosition(Location);
			if (!std::isfinite(Relative.Location.X) || !std::isfinite(Relative.Location.Y) ||
				!std::isfinite(Relative.Location.Z)) return false;
			Relative.Rotation = (Parent->GetWorldRotation().Quaternion().Inverse() *
				Rotation.Quaternion()).ToFRotator();
		}
		else
		{
			Relative.Location = Location;
			Relative.Rotation = Rotation;
		}
		Camera.SetTransform(Relative);
		return true;
	}
}

FSceneView FGameViewportClient::CalcSceneView(const FRect& InViewRect)
{
	FSceneView OutView;
	OutView.ViewIndex = 0;
	OutView.bGameView = !bSIEMode;
	OutView.ViewRect = InViewRect;

	Resize(
		static_cast<uint32>(std::max(InViewRect.Width, 1.0f)),
		static_cast<uint32>(std::max(InViewRect.Height, 1.0f))
	);

	const float Aspect = static_cast<float>(Width) / static_cast<float>(Height);

	if (!CameraComponent && World)
	{
		CameraComponent = FindPlayerCamera();

		if (!CameraComponent && World->GetMainCamera())
			CameraComponent = World->GetMainCamera()->GetCameraComponent();
	}

	if (!bSIEMode && World)
	{
		APlayerController* PC = World->GetPlayerController();
		APlayerCameraManager* PCM = PC ? PC->GetCameraManager() : nullptr;
		if (PCM && PCM->HasCameraCache())
		{
			FMinimalViewInfo POV = PCM->GetCameraCachePOV();
			POV.AspectRatio = Aspect;

			// 위치·회전·행렬
			OutView.ViewLocation = POV.Location;
			OutView.ViewRotation = POV.Rotation;
			OutView.ViewForward = POV.Rotation.Quaternion().GetForwardVector();
			OutView.ViewMatrix = POV.CalculateViewMatrix();
			OutView.ProjectionMatrix = POV.CalculateProjectionMatrix();
			OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;
			// 렌즈 값
			OutView.FieldOfView = POV.FOV;
			OutView.AspectRatio = Aspect;
			OutView.NearClip = POV.NearClip;
			OutView.FarClip = POV.FarClip;
			OutView.OrthoWidth = POV.OrthoWidth;
			OutView.bIsPerspective = !POV.bIsOrthogonal;
			OutView.bIsWireframe = false;

			// 프러스텀 후 반환
			OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);
			OutView.FrustumCorners = GenerateFrustumCorners(OutView.ViewProjectionMatrix.Inverse());
			if (PCM->bEnableFading)
			{
				OutView.OverlayColor = PCM->FadeColor;
				OutView.OverlayColor.A = FMath::Clamp(PCM->FadeAmount, 0.0f, 1.0f);
			}
			OutView.LetterboxAmount = PCM->LetterboxAmount;
			OutView.LetterboxAspect = PCM->LetterboxAspect;
			OutView.VignetteIntensity = PCM->VignetteIntensity;

			// 시퀀서 Fade 트랙 / Settings 슬라이더가 PCM 값을 덮어씀
			if (GCameraOverlayDebug.bOverride)
			{
				const FCameraOverlayDebug& D = GCameraOverlayDebug;
				OutView.OverlayColor = FLinearColor{ D.FadeColor[0], D.FadeColor[1], D.FadeColor[2], D.FadeAmount };
				OutView.LetterboxAmount = D.LetterboxAmount;
				OutView.LetterboxAspect = D.LetterboxAspect;
				OutView.VignetteIntensity = D.VignetteIntensity;
			}

			return OutView;
		}
	}

	if (CameraComponent)
	{
		CameraComponent->SetAspectRatio(Aspect);

		OutView.ViewLocation = CameraComponent->GetWorldLocation();
		OutView.ViewRotation = CameraComponent->GetWorldRotation();
		OutView.ViewForward = OutView.ViewRotation.Quaternion().GetForwardVector();
		OutView.ViewMatrix = CameraComponent->GetViewMatrix();
		OutView.ProjectionMatrix = CameraComponent->GetProjectionMatrix();
		OutView.ViewProjectionMatrix = CameraComponent->GetViewProjectionMatrix();
		OutView.FieldOfView = CameraComponent->GetFieldOfView();
		OutView.AspectRatio = Aspect;
		OutView.NearClip = CameraComponent->GetNearZ();
		OutView.FarClip = CameraComponent->GetFarZ();
		OutView.OrthoWidth = CameraComponent->GetOrthoWidth();
		OutView.bIsPerspective = !CameraComponent->GetIsOrthogonal();
		OutView.bIsWireframe = false;
	}
	else
	{
		OutView.ViewLocation = FVector(-5.0f, -10.0f, 10.0f);
		OutView.ViewRotation = FRotator(30.0f, 45.0f, 0.0f);
		OutView.ViewForward = OutView.ViewRotation.Quaternion().GetForwardVector();
		OutView.FieldOfView = 60.0f;
		OutView.AspectRatio = Aspect;
		OutView.NearClip = 0.1f;
		OutView.FarClip = 1000.0f;
		OutView.bIsPerspective = true;
		OutView.bIsWireframe = false;
	}

	OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);
	OutView.FrustumCorners = GenerateFrustumCorners(OutView.ViewProjectionMatrix.Inverse());

	if (GCameraOverlayDebug.bOverride)
	{
		const FCameraOverlayDebug& D = GCameraOverlayDebug;
		OutView.OverlayColor = FLinearColor{ D.FadeColor[0], D.FadeColor[1], D.FadeColor[2], D.FadeAmount };
		OutView.LetterboxAmount = D.LetterboxAmount;
		OutView.LetterboxAspect = D.LetterboxAspect;
		OutView.VignetteIntensity = D.VignetteIntensity;
	}


	return OutView;
}

void FGameViewportClient::SetCameraComponent(UCameraComponent* InCameraComponent)
{
	CameraComponent = InCameraComponent;
}

bool FGameViewportClient::BeginSIEMode()
{
	if (bSIEMode)
		return true;
	if (!World || !World->GetMainCamera())
		return false;

	if (!CameraComponent) CameraComponent = FindPlayerCamera();
	if (!CameraComponent) return false;

	UCameraComponent* MainCamera = World->GetMainCamera()->GetCameraComponent();
	if (!MainCamera || MainCamera == CameraComponent) return false;
	if (!SetCameraWorldPose(*MainCamera, CameraComponent->GetWorldLocation(),
		CameraComponent->GetWorldRotation())) return false;

	MainCamera->SetFieldOfView(CameraComponent->GetFieldOfView());
	MainCamera->SetNearZ(CameraComponent->GetNearZ());
	MainCamera->SetFarZ(CameraComponent->GetFarZ());
	MainCamera->SetIsOrthogonal(CameraComponent->GetIsOrthogonal());
	MainCamera->SetOrthoWidth(CameraComponent->GetOrthoWidth());
	bPreviousMainCameraExternalInputManaged = MainCamera->IsExternalInputManaged();
	MainCamera->SetExternalInputManaged(true);
	CameraComponent = MainCamera;
	bSIEMode = true;
	LostFocus();
	return true;
}

void FGameViewportClient::EndSIEMode()
{
	if (!bSIEMode)
		return;
	if (World && World->GetMainCamera())
		if (UCameraComponent* MainCamera = World->GetMainCamera()->GetCameraComponent())
			MainCamera->SetExternalInputManaged(bPreviousMainCameraExternalInputManaged);
	CameraComponent = FindPlayerCamera();
	bSIEMode = false;
	LostFocus();
}

void FGameViewportClient::GetKeyInputBySIEMode(int32 Key, bool bDown)
{
	if (bSIEMode && Key >= 0 && Key < 256)
		bSIEKeyDown[Key] = bDown;
}

void FGameViewportClient::GetAxisInputBySIEMode(EGameInputAxis Key, float Delta)
{
	if (!bSIEMode)
		return;
	switch (Key)
	{
	case EGameInputAxis::MouseX: SIEMouseDeltaX += Delta; break;
	case EGameInputAxis::MouseY: SIEMouseDeltaY += Delta; break;
	case EGameInputAxis::MouseWheel: SIEWheelDelta += Delta; break;
	}
}

void FGameViewportClient::TickSIEInput(float DeltaTime, float MoveSpeed, float MouseSensitivity)
{
	if (!bSIEMode || !CameraComponent)
		return;
	FViewportCameraInput Input;
	Input.bCaptured = bSIEKeyDown[static_cast<int32>(EKeyCode::RButton)];
	Input.bAllowWheel = true;
	Input.MouseX = SIEMouseDeltaX;
	Input.MouseY = SIEMouseDeltaY;
	Input.Wheel = SIEWheelDelta;
	Input.bForward = bSIEKeyDown[static_cast<int32>(EKeyCode::W)];
	Input.bBackward = bSIEKeyDown[static_cast<int32>(EKeyCode::S)];
	Input.bRight = bSIEKeyDown[static_cast<int32>(EKeyCode::D)];
	Input.bLeft = bSIEKeyDown[static_cast<int32>(EKeyCode::A)];
	Input.bUp = bSIEKeyDown[static_cast<int32>(EKeyCode::E)];
	Input.bDown = bSIEKeyDown[static_cast<int32>(EKeyCode::Q)];
	FViewportCameraState Camera{CameraComponent->GetWorldLocation(),
		CameraComponent->GetWorldRotation(), CameraComponent->GetOrthoWidth(),
		Width > 0 ? static_cast<float>(Width) : 800.0f, !CameraComponent->GetIsOrthogonal()};
	ApplyViewportCameraMovement(Camera, Input, DeltaTime, MoveSpeed, MouseSensitivity);
	if (SetCameraWorldPose(*CameraComponent, Camera.Location, Camera.Rotation))
		CameraComponent->SetOrthoWidth(Camera.OrthoWidth);
	SIEMouseDeltaX = SIEMouseDeltaY = SIEWheelDelta = 0.0f;
}

void FGameViewportClient::Resize(uint32 InWidth, uint32 InHeight)
{
	if (Width == InWidth && Height == InHeight && ViewTargets.IsValidFor(InWidth, InHeight))
	{
		return;
	}

	Width = InWidth;
	Height = InHeight;

	if (Width == 0 || Height == 0)
	{
		ViewTargets.Reset();
		return;
	}

	ViewTargets.Resize(Width, Height);
}

void FGameViewportClient::Reset()
{
	// PIE가 끝나면 화면에 남은 위젯을 정리한다.
	ClearViewportWidgets();

	RenderData.ResetScene();
	EndSIEMode();
	LostFocus();
	bExitRequested = false;
	bSIEModeRequested = false;
	World = nullptr;
	GameInstance = nullptr;
	Engine = nullptr;
	CameraComponent = nullptr;

	ViewTargets.Reset();
	Width = 0;
	Height = 0;
}

void FGameViewportClient::AddViewportWidget(UUserWidget* Widget)
{
	if (!Widget || ViewportWidgets.Find(Widget) != INDEX_NONE) return;
	Widget->OwningViewport = this;

	// ZOrder가 같으면 나중에 추가한 것이 위에 온다.
	int32 Index = 0;
	while (Index < ViewportWidgets.Num() && ViewportWidgets[Index]->ZOrder <= Widget->ZOrder)
		++Index;
	ViewportWidgets.Insert(Widget, Index);
}

void FGameViewportClient::RemoveViewportWidget(UUserWidget* Widget)
{
	const int32 Index = ViewportWidgets.Find(Widget);
	if (Index == INDEX_NONE) return;
	ViewportWidgets.RemoveAt(Index);
	Widget->OwningViewport = nullptr;

	// 빠진 트리 안을 가리키고 있을 수 있으니 비운다.
	HoveredWidget = nullptr;
	PressedWidget = nullptr;
}

void FGameViewportClient::ClearViewportWidgets()
{
	const TArray<UUserWidget*> Widgets = ViewportWidgets;
	for (UUserWidget* Widget : Widgets)
		Widget->Destroy();
	ViewportWidgets.Empty();
	HoveredWidget = nullptr;
	PressedWidget = nullptr;
}

void FGameViewportClient::TickUI(float DeltaTime)
{
	// Tick 중에 위젯이 스스로 빠질 수 있어 복사본으로 돈다.
	const TArray<UUserWidget*> Widgets = ViewportWidgets;
	for (UUserWidget* Widget : Widgets)
		Widget->Tick(DeltaTime);
}

void FGameViewportClient::PaintUI(FUIDrawList& DrawList)
{
	DrawList.Reset();
	if (Width == 0 || Height == 0) return;

	const FUIGeometry Screen{ FUIRect{ 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height) },
		static_cast<float>(Height) / UIDesignHeight };
	for (UUserWidget* Widget : ViewportWidgets)
	{
		Widget->Arrange(Screen);
		Widget->Paint(DrawList, 1.0f);
	}
}

UWidget* FGameViewportClient::HitTestUI(float X, float Y) const
{
	for (auto It = ViewportWidgets.rbegin(); It != ViewportWidgets.rend(); ++It)
		if (UWidget* Hit = (*It)->HitTest(X, Y))
			return Hit;
	return nullptr;
}

bool FGameViewportClient::HandleUIMouseMove(int32 X, int32 Y)
{
	UIMouseX = X;
	UIMouseY = Y;

	UWidget* Hit = HitTestUI(static_cast<float>(X), static_cast<float>(Y));
	if (Hit != HoveredWidget)
	{
		if (HoveredWidget) HoveredWidget->OnMouseLeave();
		HoveredWidget = Hit;
		if (HoveredWidget) HoveredWidget->OnMouseEnter();
	}
	return false;   // 마우스 이동은 카메라도 써야 하므로 소비하지 않는다
}

bool FGameViewportClient::HandleUIKey(int32 Key, bool bDown)
{
	if (Key != static_cast<int32>(EKeyCode::LButton)) return false;

	const float X = static_cast<float>(UIMouseX);
	const float Y = static_cast<float>(UIMouseY);
	if (bDown)
	{
		UWidget* Hit = HitTestUI(X, Y);
		if (Hit && Hit->OnMouseButtonDown())
		{
			PressedWidget = Hit;
			return true;    // 버튼을 눌렀으면 게임으로 넘기지 않는다
		}
		return false;
	}

	if (!PressedWidget) return false;
	PressedWidget->OnMouseButtonUp(PressedWidget->GetCachedRect().Contains(X, Y));
	PressedWidget = nullptr;
	return true;
}

bool FGameViewportClient::HandleUIAxis(EGameInputAxis AxisKey, float Delta)
{
	return false;   // 스크롤 박스를 만들 때 휠을 여기서 처리한다
}
