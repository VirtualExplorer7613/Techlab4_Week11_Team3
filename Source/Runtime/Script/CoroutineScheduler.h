#pragma once

#include <sol/forward.hpp>

class AActor;

// wait(sec), wait_until(cond) 등으로 멈춘 코루틴을 들고 있다가 조건이 되면 재개한다. 담당: B
class FCoroutineScheduler
{
public:
	static FCoroutineScheduler& Get();

	void Start(AActor* Owner, sol::function Func);
	void Tick(float DeltaTime);     // A가 월드 Tick에서 1회 호출
	void StopAll(AActor* Owner);    // A가 액터 EndPlay·Destroy에서 호출

	void MoveTo(AActor* Owner, const FVector& Target, float Speed);
	bool IsMoving(const AActor* Owner) const;

private:
	struct FCoroutine;
	struct FMoveTask;

	bool Resume(FCoroutine& Coroutine);
	void TickMoves(float DeltaTime);

	TArray<TUniquePtr<FCoroutine>> Coroutines;
	TArray<TUniquePtr<FCoroutine>> Pending;		// Tick 도중 새로 시작된 것. 다음 프레임부터 돈다
	TArray<FMoveTask> Moves;
	bool bTicking = false;
};