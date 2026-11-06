#pragma once
#include "MovementComponent.h"
#include "PatrolMovementComponent.generated.h"

// 시작 위치와 PatrolPoint1~3을 차례로 오가는 이동. 장애물마다 스크립트를 붙이지 않고 Details에서 경로만 고친다.
// PatrolPoint는 시작 위치 기준 오프셋이라 액터를 옮기면 경로도 함께 옮겨진다.
// bLoop이면 마지막 지점 다음에 시작 위치로 돌아가 같은 방향으로 돌고, 아니면 왔던 길을 되돌아간다.
// 움직인 뒤 블로킹 컴포넌트가 SlidingMovement 액터에 닿으면 벽 충돌과 같은 경로(HandleImpact → OnHit)로 알린다.
// 그래서 가만히 서 있는 플레이어도 지나가는 장애물에 맞으면 Lua의 기존 사망 규칙이 적용된다.
UCLASS()
class UPatrolMovementComponent : public UMovementComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FVector PatrolPoint1 = FVector::ZeroVector;
	UPROPERTY(EditAnywhere)
	FVector PatrolPoint2 = FVector::ZeroVector;
	UPROPERTY(EditAnywhere)
	FVector PatrolPoint3 = FVector::ZeroVector;
	UPROPERTY(EditAnywhere)
	int PatrolPointCount = 1; // 사용할 PatrolPoint 개수(0~3). 0이면 움직이지 않는다.
	UPROPERTY(EditAnywhere)
	float Speed = 3.0f;
	UPROPERTY(EditAnywhere)
	bool bLoop = false;
	UPROPERTY(EditAnywhere)
	bool bFaceMoveDirection = true;

	virtual void TickComponent(float DeltaTime) override;

private:
	int32 GetPathPointNum() const;
	FVector GetPathPoint(int32 Index) const;
	void AdvanceTarget();
	void NotifyTouchedMovers();

	FVector StartLocation = FVector::ZeroVector;
	int32 TargetIndex = 1;
	int32 Direction = 1;
	bool bStarted = false;
};
