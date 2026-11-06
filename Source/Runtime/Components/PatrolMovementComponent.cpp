#include "EnginePCH.h"
#include "PatrolMovementComponent.h"

#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SlidingMovementComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"
#include <algorithm>
#include <cmath>

void UPatrolMovementComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	// 에디터 월드에서는 배치한 자리에 그대로 둔다.
	if (UpdatedComponent == nullptr || !GetWorld() || !GetWorld()->IsGameWorld() || GetPathPointNum() < 2 || Speed <= 0.0f)
	{
		return;
	}

	if (!bStarted)
	{
		StartLocation = UpdatedComponent->GetWorldLocation();
		bStarted = true;
	}

	const FVector OldLocation = UpdatedComponent->GetWorldLocation();
	FVector NewLocation = OldLocation;
	float Remaining = Speed * DeltaTime;

	// 한 프레임에 여러 지점을 지날 수 있다. 모든 지점이 겹친 경로에서 멈추지 않도록 횟수를 제한한다.
	for (int32 Step = 0; Step < 8 && Remaining > 0.0f; ++Step)
	{
		const FVector ToTarget = GetPathPoint(TargetIndex) - NewLocation;
		const float Distance = ToTarget.Length();
		if (Distance > Remaining)
		{
			NewLocation = NewLocation + ToTarget * (Remaining / Distance);
			break;
		}
		NewLocation = GetPathPoint(TargetIndex);
		Remaining -= Distance;
		AdvanceTarget();
	}

	const FVector Delta = NewLocation - OldLocation;
	FRotator NewRotation = UpdatedComponent->GetWorldRotation();
	if (bFaceMoveDirection && Delta.SizeSquared() > SMALL_NUMBER)
	{
		NewRotation = FRotator(0.0f, FMath::RadiansToDegrees(std::atan2(Delta.Y, Delta.X)), 0.0f);
	}

	// 장애물은 벽에 막히지 않는다. 플레이어가 장애물로 들어가는 충돌은 플레이어 쪽 Sweep이,
	// 장애물이 플레이어로 들어가는 충돌은 아래 NotifyTouchedMovers가 맡는다.
	MoveUpdatedComponent(Delta, NewRotation, false, nullptr);
	NotifyTouchedMovers();
}

void UPatrolMovementComponent::NotifyTouchedMovers()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetActorEnableCollision())
	{
		return;
	}

	for (UActorComponent* Component : Owner->GetComponents())
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!Primitive || !Primitive->bBlockComponent)
		{
			continue;
		}

		// 플레이어 Sweep이 장애물을 판정할 때와 같은 기준(블로킹 컴포넌트 AABB + 이동 반경)을 쓴다.
		const FBox Bounds = Primitive->CalcBounds();
		for (TObjectIterator<USlidingMovementComponent> It; It; ++It)
		{
			USlidingMovementComponent* Mover = *It;
			AActor* MoverOwner = Mover ? Mover->GetOwner() : nullptr;
			// 이미 죽어 이동이 꺼진 플레이어에게는 다시 알리지 않는다.
			if (!MoverOwner || MoverOwner == Owner || !Mover->IsRegistered() || Mover->GetWorld() != GetWorld() ||
				!MoverOwner->GetActorEnableCollision() || !Mover->IsMovementEnabled())
			{
				continue;
			}

			const FVector Center = MoverOwner->GetActorLocation();
			const FVector Closest(
				std::clamp(Center.X, Bounds.Min.X, Bounds.Max.X),
				std::clamp(Center.Y, Bounds.Min.Y, Bounds.Max.Y),
				std::clamp(Center.Z, Bounds.Min.Z, Bounds.Max.Z));
			const float Radius = std::max(Mover->CollisionRadius, 0.0f);
			if ((Center - Closest).SizeSquared() > Radius * Radius)
			{
				continue;
			}

			FHitResult Hit(1.0f);
			Hit.bBlockingHit = true;
			Hit.HitComponent = Primitive;
			Hit.ImpactPoint = Closest;
			Mover->HandleImpact(Hit, 0.0f, FVector::ZeroVector);
		}
	}
}

int32 UPatrolMovementComponent::GetPathPointNum() const
{
	return 1 + std::clamp(PatrolPointCount, 0, 3);
}

FVector UPatrolMovementComponent::GetPathPoint(int32 Index) const
{
	switch (Index)
	{
	case 1: return StartLocation + PatrolPoint1;
	case 2: return StartLocation + PatrolPoint2;
	case 3: return StartLocation + PatrolPoint3;
	default: return StartLocation;
	}
}

void UPatrolMovementComponent::AdvanceTarget()
{
	const int32 Num = GetPathPointNum();
	if (bLoop)
	{
		TargetIndex = (TargetIndex + 1) % Num;
		return;
	}

	if (TargetIndex + Direction < 0 || TargetIndex + Direction >= Num)
	{
		Direction = -Direction;
	}
	TargetIndex += Direction;
}
