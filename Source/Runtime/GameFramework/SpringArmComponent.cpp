#include "EnginePCH.h"
#include "SpringArmComponent.h"

#include <cmath>

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Render/LineBatcher.h"
#include "UObject/UObjectIterator.h"

USpringArmComponent::USpringArmComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::PostPhysics; // 폰 이동 후
}

void USpringArmComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	UpdateDesiredArmLocation(
		bDoCollisionTest,
		bEnableCameraLag,
		bEnableCameraRotationLag,
		DeltaTime);
}

void USpringArmComponent::UpdateInEditor()
{
	// 편집중에는 lag 없이 바로 적용
	UpdateDesiredArmLocation(
		bDoCollisionTest,
		false,
		false,
		0.0f);
}

void USpringArmComponent::ResetCameraLag()
{
	UpdateDesiredArmLocation(
		bDoCollisionTest,
		false,
		false,
		0.0f);
}

void USpringArmComponent::UpdateDesiredArmLocation(
	bool bDoTrace,
	bool bDoLocationLag,
	bool bDoRotationLag,
	float DeltaTime)
{
	// 1) 회전 (+ Rotation Lag). UE는 쿼터니언 QInterpTo (최단 경로, 짐벌락 없음)
	FRotator DesiredRot = GetTargetRotation();
	if (bDoRotationLag)
	{
		DesiredRot = FMath::QInterpTo(
				PreviousDesiredRot.Quaternion(),
				DesiredRot.Quaternion(),
		 		DeltaTime,
				CameraRotationLagSpeed).ToFRotator();
	}
	PreviousDesiredRot = DesiredRot;

	// 2) 팔 시작점 (+ Location Lag는 "시작점"에 건다: 팔 길이는 유지된 채 늦게 따라옴
	const FVector ArmOrigin = GetWorldLocation() + TargetOffset;
	FVector DesiredLoc = ArmOrigin;
	if (bDoLocationLag)
	{
		DesiredLoc = FMath::VInterpTo(
			PreviousDesiredLoc,
			ArmOrigin,
			DeltaTime,
			CameraLagSpeed);
		const FVector FromOrigin = DesiredLoc - ArmOrigin;
		if (CameraLagMaxDistance > 0.0f &&
			FromOrigin.Size() > CameraLagMaxDistance)
		{
			DesiredLoc = ArmOrigin
				+ FromOrigin.GetSafeNormal() * CameraLagMaxDistance;
		}
	}
	PreviousDesiredLoc = DesiredLoc;

	// 3) 팔을 뒤로 뻗고 소켓 오프셋 적용
	const FQuat Q = DesiredRot.Quaternion();
	DesiredLoc -= Q.GetForwardVector() * TargetArmLength;
	DesiredLoc += Q.RotateVector(SocketOffset);

	// 4) 충돌: 시작점 -> 원하는 위치로 구를 쓸어서, 막히면 맞은 거리에서 멈춘다
	FVector ResultLoc = DesiredLoc;
	bool bCollisionHit = false;

	if (bDoTrace && TargetArmLength > 0.0f)
	{
		const FVector ToCamera = DesiredLoc - ArmOrigin;
		const float Len = ToCamera.Size();

		FHitResult Hit;
		if (Len > 1.e-4f &&
			GetWorld()->SweepSingle(FRay(ArmOrigin, ToCamera / Len), Len, ProbeSize, Hit, GetOwner(), true))
		{
			ResultLoc = Hit.Location;
			bCollisionHit = true;
		}
	}

	// 디버그용 데이터 저장
	DebugArmOrigin = ArmOrigin;
	DebugDesiredLoc = DesiredLoc;
	DebugResultLoc = ResultLoc;
	bDebugCollisionHit = bCollisionHit;
	bDebugHasData = true;

	// 5) 자식 카메라에 반영.
	for (USceneComponent* Child : GetAttachChildren())
	{
		if (Child)
		{
			Child->SetWorldLocationAndRotation(ResultLoc, DesiredRot, false);
		}
	}
}

void USpringArmComponent::OnRegister()
{
	Super::OnRegister();
	UpdateDesiredArmLocation(false, false, false, 0.0f);
}

FRotator USpringArmComponent::GetTargetRotation() const
{
	FRotator R = GetWorldRotation();
	if (bUsePawnControlRotation)
	{
		if (APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			if (APlayerController* PC = Pawn->GetController())
			{
				R = PC->ControlRotation;
			}
		}
	}

	const FRotator Rel = GetRelativeRotation();
	if (!bInheritPitch)
	{
		R.Pitch = Rel.Pitch;
	}
	if (!bInheritYaw)
	{
		R.Yaw = Rel.Yaw;
	}
	if (!bInheritRoll)
	{
		R.Roll = Rel.Roll;
	}

	return R;
}

void USpringArmComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	if (!LineBatcher || !bDrawDebug || !bDebugHasData)
	{
		return;
	}

	const FVector4 Cyan(0.0f, 1.0f, 1.0f, 1.0f);
	const FVector4 Green(0.2f, 1.0f, 0.2f, 1.0f);
	const FVector4 Red(1.0f, 0.2f, 0.2f, 1.0f);
	const FVector4 Yellow(1.0f, 1.0f, 0.0f, 1.0f);

	auto DrawCross = [&](
		const FVector& Center,
		float Size,
		const FVector4& Color)
		{
			const FVector Axes[] = {
				FVector(Size, 0.0f, 0.0f),
				FVector(0.0f, Size, 0.0f),
				FVector(0.0f, 0.0f, Size)
			};

			for (const FVector& Axis : Axes)
			{
				LineBatcher->AddLine(Center - Axis, Center + Axis, Color);
			}
		};

	auto DrawCircle = [&](
		const FVector& Center,
		const FVector& AxisA,
		const FVector& AxisB,
		float Radius,
		const FVector4& Color
		)
		{
			constexpr int32 Segments = 24;
			constexpr float TwoPi = 2.0f * 3.14159265358979323846f;

			FVector Previous = Center + AxisA * Radius;

			for (int32 i = 1; i <= Segments; ++i)
			{
				const float Angle = TwoPi * static_cast<float>(i) / Segments;
				const FVector Point =
					Center
					+ AxisA * (std::cos(Angle) * Radius)
					+ AxisB * (std::sin(Angle) * Radius);

				LineBatcher->AddLine(Previous, Point, Color);
				Previous = Point;
			}
		};

	// 실제 팔 길이
	LineBatcher->AddLine(DebugArmOrigin, DebugResultLoc, Green);

	// 충돌로 인해 도달하지 못한 구간
	if (bDebugCollisionHit)
	{
		LineBatcher->AddLine(DebugResultLoc, DebugDesiredLoc, Red);
	}

	DrawCross(DebugArmOrigin, 0.08f, Cyan);
	DrawCross(DebugDesiredLoc, 0.08f, Yellow);

	// 실제 끝점에 충돌 검사 구를 표시
	if (bDoCollisionTest && TargetArmLength > 0.0f && ProbeSize > 0.0f)
	{
		const FVector4 ProbeColor = bDebugCollisionHit ? Red : Green;

		DrawCircle(
			DebugResultLoc,
			FVector(1, 0, 0), FVector(0, 1, 0),
			ProbeSize, ProbeColor);

		DrawCircle(
			DebugResultLoc,
			FVector(1, 0, 0), FVector(0, 0, 1),
			ProbeSize, ProbeColor);

		DrawCircle(
			DebugResultLoc,
			FVector(0, 1, 0), FVector(0, 0, 1),
			ProbeSize, ProbeColor);
	}
	else
	{
		DrawCross(DebugResultLoc, 0.06f, Green);
	}
}

void DrawSpringArms(
	UWorld* World,
	FLineBatcher* LineBatcher,
	const AActor* Selected
)
{
	if (!World || !LineBatcher)
	{
		return;
	}

	for (TObjectIterator<USpringArmComponent> It; It; ++It)
	{
		USpringArmComponent* Arm = *It;

		if (!Arm ||
			Arm->GetWorld() != World ||
			!Arm->IsRegistered())
		{
			continue;
		}

		if (Arm->bDrawOnlyIfSelected &&
			Arm->GetOwner() != Selected)
		{
			continue;
		}

		Arm->DrawDebug(LineBatcher);
	}
}