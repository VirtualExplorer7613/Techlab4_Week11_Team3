#include "EnginePCH.h"
#include "Script/CoroutineScheduler.h"

#include <sol/sol.hpp>

#include "Script/LuaScriptManager.h"
#include "Script/LuaActorRef.h"
#include "GameFramework/Actor.h"

enum class EWaitKind
{
	None,
	Time,
	Until,
	Move,
	Frame
};

struct FCoroutineScheduler::FCoroutine
{
	AActor* Owner = nullptr;			// StopAll 비교용
	sol::thread Thread;					// 이 객체가 살아 있는 동안 Lua 스택도 GC되지 않는다.
	EWaitKind Wait = EWaitKind::None;
	float TimeLeft = 0.0f;
	sol::protected_function Condition;	// wait_until(fn)
	bool bDead = false;
};

struct FCoroutineScheduler::FMoveTask
{
	FLuaActorRef Owner;
	FVector Target;
	float Speed = 1.0f;
};

FCoroutineScheduler& FCoroutineScheduler::Get()
{
	static FCoroutineScheduler Instance;
	return Instance;
}

void FCoroutineScheduler::Start(AActor* Owner, sol::function Func)
{
	sol::state& Lua = FLuaScriptManager::Get().GetState();

	auto Co = MakeUnique<FCoroutine>();
	Co->Owner = Owner;
	Co->Thread = sol::thread::create(Lua.lua_state());
	Func.push(Co->Thread.thread_state());

	// 첫 yield는 바로 실행한다.
	if (!Resume(*Co))
	{
		return;	// yield 없이 끝났거나 에러
	}

	// Tick 중이면 Pending에 넣고, 아니면 바로 Coroutines에 넣는다.
	(bTicking ? Pending : Coroutines).Add(std::move(Co));
}

namespace
{
	// yield된 스레드에서는 함수를 호출할 수 없다. 값을 메인 스레드로 옮겨 참조를 만든다.
	sol::protected_function TakeFunction(lua_State* Co, int Index, lua_State* Main)
	{
		lua_pushvalue(Co, Index);
		lua_xmove(Co, Main, 1);
		sol::protected_function F(Main, -1);
		lua_pop(Main, 1);
		return F;
	}
}

// 계속 돌아야 하면 true, 끝났거나 에러면 false
bool FCoroutineScheduler::Resume(FCoroutine& Coroutine)
{
	lua_State* Main = FLuaScriptManager::Get().GetState().lua_state();
	lua_State* Co = Coroutine.Thread.thread_state();

	int NumResults = 0; // 출력 매개변수 개수
	const int Status = lua_resume(Co, Main, 0, &NumResults);

	if (Status == LUA_OK)
	{
		return false; // 정상 종료
	}

	if (Status != LUA_YIELD)
	{
		luaL_traceback(Main, Co, lua_tostring(Co, -1), 0);
		HTR_LOG(Error, "[Lua] 코루틴 오류\n{}", lua_tostring(Main, -1));
		lua_pop(Main, 1);
		return false;
	}

	// yield가 남긴 값: ("time", 1.0) / ("until", fn) / ("move") / ()
	const int Base = lua_gettop(Co) - NumResults + 1;
	const char* Kind = NumResults >= 1 ? lua_tostring(Co, Base) : nullptr;
	Coroutine.Wait = EWaitKind::Frame;
	if (Kind && std::strcmp(Kind, "time") == 0)
	{
		Coroutine.Wait = EWaitKind::Time;
		Coroutine.TimeLeft = NumResults >= 2
			? static_cast<float>(lua_tonumber(Co, Base + 1))
			: 0.0f;
	}
	else if (Kind && std::strcmp(Kind, "until") == 0 &&
		NumResults >= 2 &&
		lua_isfunction(Co, Base + 1))
	{
		Coroutine.Wait = EWaitKind::Until;
		Coroutine.Condition = TakeFunction(Co, Base + 1, Main);
	}
	else if (Kind && std::strcmp(Kind, "move") == 0)
	{
		Coroutine.Wait = EWaitKind::Move;
	}
	lua_pop(Co, NumResults);
	return true;
}

void FCoroutineScheduler::Tick(float DeltaTime)
{
	TickMoves(DeltaTime);

	bTicking = true;
	for (TUniquePtr<FCoroutine>& Ptr : Coroutines)
	{
		FCoroutine& C = *Ptr;
		if (C.bDead)
			continue;

		bool bReady = false;
		switch (C.Wait)
		{
		case EWaitKind::Time:
		{
			C.TimeLeft -= DeltaTime;
			bReady = C.TimeLeft <= 0.0f;
			break;
		}
		case EWaitKind::Until:
		{
			sol::protected_function_result R = C.Condition();
			if (!R.valid())
			{
				sol::error E = R;
				HTR_LOG(Error, "[Lua] wait_until 조건 오류\n{}", E.what());
				C.bDead = true;
				continue;
			}
			bReady = R.get<bool>();
			break;
		}
		case EWaitKind::Move:
		{
			bReady = !IsMoving(C.Owner);
			break;
		}
		default:
			bReady = true;
			break;
		}

		if (bReady && !Resume(C))
		{
			C.bDead = true;
		}
	}
	bTicking = false;

	// 끝난 것 정리, Tick 도중 시작된 것 합류
	for (int32 i = Coroutines.Num() - 1; i >= 0; --i)
	{
		if (Coroutines[i]->bDead)
		{
			Coroutines.RemoveAt(i, 1);
		}
	}
	for (TUniquePtr<FCoroutine>& P : Pending)
	{
		Coroutines.Add(std::move(P));
	}
	Pending = TArray<TUniquePtr<FCoroutine>>();
}
void FCoroutineScheduler::StopAll(AActor* Owner)
{
	for (TUniquePtr<FCoroutine>& C : Coroutines)
	{
		if (C->Owner == Owner)
		{
			C->bDead = true;
		}
	}
	for (TUniquePtr<FCoroutine>& C : Pending)
	{
		if (C->Owner == Owner)
		{
			C->bDead = true;
		}
	}
	for (int32 i = Moves.Num() - 1; i >= 0; --i)
	{
		if (Moves[i].Owner.Ptr == Owner)
		{
			Moves.RemoveAt(i, 1);
		}
	}
}

void FCoroutineScheduler::MoveTo(AActor* Owner, const FVector& Target, float Speed)
{
	for (FMoveTask& M : Moves)
	{
		if (M.Owner.Ptr == Owner)
		{
			M.Target = Target;
			M.Speed = Speed;
			return;
		}
	}
	Moves.Add({ FLuaActorRef::Make(Owner), Target, Speed });
}

bool FCoroutineScheduler::IsMoving(const AActor* Owner) const
{
	for (const FMoveTask& M : Moves)
	{
		if (M.Owner.Ptr == Owner)
		{
			return true;
		}
	}
	return false;
}

void FCoroutineScheduler::TickMoves(float DeltaTime)
{
	for (int32 i = Moves.Num() - 1; i >= 0; --i)
	{
		FMoveTask& M = Moves[i];
		AActor* Actor = M.Owner.Get();
		USceneComponent* Root = Actor
			? Actor->GetRootComponent()
			: nullptr;

		if (!Root)
		{
			Moves.RemoveAt(i, 1);
			continue;
		}

		const FVector Cur = Actor->GetActorLocation();
		const FVector ToTarget = M.Target - Cur;
		const float Dist = ToTarget.Length();
		const float Step = M.Speed * DeltaTime;
		if (Dist <= Step)
		{
			Root->SetWorldLocation(M.Target, false);
			Moves.RemoveAt(i, 1); // 도착 -> wait_until_move_done이 풀린다.
		}
		else
		{
			Root->SetWorldLocation(Cur + ToTarget * (Step / Dist), false);
		}
	}
}

