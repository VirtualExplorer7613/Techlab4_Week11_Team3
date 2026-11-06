#pragma once

#include <sol/forward.hpp>

// sol::state 하나를 엔진 전체가 공유한다. 액터별 격리는 FLuaScriptInstance의 environment로 한다.
// 담당: A, 소비자: B (FLuaBindings::Register에 GetState를 넘긴다)
class FLuaScriptManager
{
public:
	static FLuaScriptManager& Get();

	// open_libraries, print → OutputLog 연결, FLuaBindings::Register 호출
	void Initialize();
	void Shutdown();

	sol::state& GetState();

	// 씬이름_액터이름.lua 규칙과 템플릿 경로
	static FString MakeScriptPath(const FString& SceneName, const FString& ActorName);
	static FString GetTemplatePath();

	// template.lua를 Path로 복제한다. 이미 있으면 덮어쓰지 않고 true
	bool CreateScriptFromTemplate(const FString& Path);
	// 기본 연결 프로그램으로 연다. ShellExecute 반환값 ≤ 32이면 false
	bool OpenScriptInEditor(const FString& Path);

	static fs::path ToPath(const FString& Utf8)
	{
		return fs::path(std::u8string(Utf8.begin(), Utf8.end()));
	}

	static FString FromPath(const fs::path& Path)
	{
		const std::u8string U8 = Path.u8string();
		return FString(U8.begin(), U8.end());
	}

	bool bEnableHotReload = true;

private:
	FLuaScriptManager() = default;
	~FLuaScriptManager();

	sol::state* State = nullptr;
};
