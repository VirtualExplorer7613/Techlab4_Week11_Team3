#include "EnginePCH.h"
#include "LuaScriptInstance.h"
#include "LuaScriptManager.h"
#include <sol/sol.hpp>
#include "GameFramework/Actor.h"
#include "LuaActorRef.h"
#include "Engine/World.h"
#include "LuaBindings.h"

struct FLuaScriptInstance::FImpl
{
	sol::environment Env;
	sol::table PersistentState;
	fs::file_time_type LastWriteTime;
	float CheckTimer = 0.0f;
	bool bTickBroken = false; // Tick에서 에러가나면 다음 Reload 전까지 Tick 멈춤
};

namespace
{
	constexpr float HotReloadInterval = 0.5f;

	bool ReadTextFile(const FString& Path, FString& OutText)
	{
		std::ifstream File(FLuaScriptManager::ToPath(Path), std::ios::binary);
		if (!File)
			return false;
		OutText.assign(std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>());
		return true;
	}

	template <typename... Ts>
	bool CallLua(sol::environment& Env, const FString& Path, const char* Name, Ts&&... Args)
	{
		if (!Env.valid())
			return true;

		sol::object Fn = Env.raw_get<sol::object>(Name);
		if (Fn.get_type() != sol::type::function)
			return true;

		sol::protected_function Func = Fn;
		sol::protected_function_result Result = Func(std::forward<Ts>(Args)...);
		if (Result.valid())
			return true;

		sol::error Err = Result;
		HTR_LOG(Error, "[Lua] {} : {}() Function Error\n{}", Path, Name, Err.what());
		return false;
	}
}

FLuaScriptInstance::FLuaScriptInstance()
	: Impl(std::make_unique<FImpl>())
{
}

FLuaScriptInstance::~FLuaScriptInstance() = default;

bool FLuaScriptInstance::Load(AActor* InOwner, const FString& InPath)
{
	Owner = InOwner;
	Path = InPath;

	FString Code;
	if (!ReadTextFile(Path, Code))
	{
		HTR_LOG(Error, "[Lua] Cannot Open Script File : {}", Path);
		return false;
	}

	std::error_code Ec;
	Impl->LastWriteTime = fs::last_write_time(FLuaScriptManager::ToPath(Path), Ec);

	sol::state& Lua = FLuaScriptManager::Get().GetState();

	sol::environment NewEnv(Lua, sol::create, Lua.globals());
	NewEnv["obj"] = FLuaActorRef::Make(Owner);
	// 액터별 Lua 상태를 핫 리로드에도 유지한다. 인스턴스가 끝나면 함께 해제된다.
	if (!Impl->PersistentState.valid()) Impl->PersistentState = Lua.create_table();
	NewEnv["script_state"] = Impl->PersistentState;

	sol::protected_function_result Result = Lua.safe_script(Code, NewEnv, sol::script_pass_on_error, "@" + Path);
	if (!Result.valid())
	{
		sol::error Err = Result;
		HTR_LOG(Error, "[Lua] {} Failed To Load\n{}", Path, Err.what());
		return false;
	}

	Impl->Env = std::move(NewEnv);
	Impl->bTickBroken = false;
	return true;
}

void FLuaScriptInstance::Tick(float DeltaTime)
{
	ReloadIfChanged(DeltaTime);

	if (Impl->bTickBroken)
		return;

	if (!CallLua(Impl->Env, Path, "Tick", DeltaTime))
	{
		Impl->bTickBroken = true;
		HTR_LOG(Warning, "[Lua] {} : Tick Broken", Path);
	}
}

void FLuaScriptInstance::ReloadIfChanged(float DeltaTime)
{
	// TODO(A): fs::last_write_time 비교 후 Load(Owner, Path)
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!FLuaScriptManager::Get().bEnableHotReload || !World || !World->IsPlayInEditor())
		return;

	Impl->CheckTimer += DeltaTime;
	if (Impl->CheckTimer < HotReloadInterval)
		return;
	Impl->CheckTimer = 0.0f;

	std::error_code Ec;
	const fs::file_time_type WriteTime = fs::last_write_time(FLuaScriptManager::ToPath(Path), Ec);
	if (Ec || WriteTime == Impl->LastWriteTime) return;

	if (Load(Owner, Path))
	{
		FCoroutineScheduler::Get().StopAll(Owner);
		FLuaBindings::ReleaseCallbacks(Owner);
		HTR_LOG(Info, "[Lua] Hot Reload: {}", Path);
		Call("OnReload");
	}
}

void FLuaScriptInstance::Call(const char* Name)
{
	CallLua(Impl->Env, Path, Name);
}

void FLuaScriptInstance::Call(const char* Name, float Value)
{
	CallLua(Impl->Env, Path, Name, Value);
}

void FLuaScriptInstance::Call(const char* Name, AActor* Other)
{
	CallLua(Impl->Env, Path, Name, FLuaActorRef::Make(Other));
}
