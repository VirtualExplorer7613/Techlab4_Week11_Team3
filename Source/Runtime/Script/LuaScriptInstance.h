#pragma once

#include <memory>

class AActor;

// 액터 하나에 붙은 Lua 스크립트. UObject가 아니다. 담당: A
// 액터마다 sol::environment를 따로 두어 Tick 같은 전역 함수가 서로 덮어쓰지 않게 한다.
// Actor.cpp에서도 include하므로 sol 타입은 cpp 안의 FImpl에 숨긴다.
struct FLuaScriptInstance
{
	FLuaScriptInstance();
	~FLuaScriptInstance();

	// env 생성, obj = Owner 주입, 파일 실행. 문법 오류면 로그 남기고 false
	bool Load(AActor* InOwner, const FString& InPath);

	void Tick(float DeltaTime);

	// 함수가 없으면 무시. Lua 에러는 protected_function으로 잡아 로그만 남긴다
	void Call(const char* Name);
	void Call(const char* Name, float Value);
	void Call(const char* Name, AActor* Other);

	AActor* GetOwner() const { return Owner; }
	const FString& GetPath() const { return Path; }

private:
	// 파일 수정 시각이 바뀌었으면 다시 Load. 매 Tick 호출해도 싸야 한다
	void ReloadIfChanged(float DeltaTime);

	struct FImpl;
	std::unique_ptr<FImpl> Impl;

	AActor* Owner = nullptr;
	FString Path;
};
