#include "EnginePCH.h"
#include "LuaScriptManager.h"

#include "LuaBindings.h"
#include <sol/sol.hpp>
#include <shellapi.h>

FLuaScriptManager& FLuaScriptManager::Get()
{
	static FLuaScriptManager Instance;
	return Instance;
}

FLuaScriptManager::~FLuaScriptManager()
{
	Shutdown();
}

void FLuaScriptManager::Initialize()
{
	if (State)
		return;

	State = new sol::state();

	State->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
		sol::lib::table, sol::lib::coroutine);

	State->set_function("print", [](sol::this_state S, sol::variadic_args Args)
		{
			sol::state_view Lua(S);
			sol::protected_function ToString = Lua["tostring"];

			FString Line;
			for (auto Arg : Args)
			{
				if (!Line.empty())
					Line += "\t";
				sol::protected_function_result Str = ToString(Arg);
				Line += Str.valid() ? Str.get<FString>() : FString("<?>");
			}
			HTR_LOG(Info, "[Lua] {}", Line);
		});

	FLuaBindings::Register(*State);
}

void FLuaScriptManager::Shutdown()
{
	delete State;
	State = nullptr;
}

sol::state& FLuaScriptManager::GetState()
{
	if (!State)
		Initialize();
	return *State;
}

FString FLuaScriptManager::MakeScriptPath(const FString& SceneName, const FString& ActorName)
{
	return "Scripts/" + SceneName + "_" + ActorName + ".lua";
}

FString FLuaScriptManager::GetTemplatePath()
{
	return "Scripts/template.lua";
}

bool FLuaScriptManager::CreateScriptFromTemplate(const FString& Path)
{
	std::error_code Ec;
	const fs::path Dest = ToPath(Path);

	// 같은 이름 파일이 이미 있으면 덮어쓰지 않고 연결만 한다 (실수로 작업한 코드를 날리지 않게)
	if (fs::exists(Dest, Ec))
	{
		HTR_LOG(Info, "[Lua] Connect To Existing Script: {}", Path);
		return true;
	}

	fs::create_directories(Dest.parent_path(), Ec);
	if (!fs::copy_file(ToPath(GetTemplatePath()), Dest, Ec))
	{
		HTR_LOG(Error, "[Lua] Failed To Copy Template: {} → {}", GetTemplatePath(), Path);
		return false;
	}

	HTR_LOG(Info, "[Lua] Create Script: {}", Path);
	return true;
}

bool FLuaScriptManager::OpenScriptInEditor(const FString& Path)
{
	std::error_code Ec;
	const fs::path Full = fs::absolute(ToPath(Path), Ec);

	auto Open = [&](const wchar_t* File, const wchar_t* Params)
		{
			HINSTANCE R = ShellExecuteW(nullptr, L"open", File, Params, nullptr, SW_SHOWNORMAL);
			return reinterpret_cast<INT_PTR>(R);
		};

	INT_PTR Result = Open(Full.c_str(), nullptr);
	if (Result == SE_ERR_NOASSOC)
	{
		// .lua에 연결된 프로그램이 없는 PC는 메모장으로 열기
		Result = Open(L"notepad.exe", Full.c_str());
	}

	if (Result <= 32)
	{
		HTR_LOG(Error, "[Lua] 편집기를 열 수 없습니다 (코드 : {}): {}", static_cast<int>(Result), Path);
		return false;
	}

	return true;
}
