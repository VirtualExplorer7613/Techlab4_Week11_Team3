#pragma once

#include <unordered_map>

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Containers/Map.h"

// [Section] / Key=Value / ;주석 형식의 INI 파일을 읽는다. 
// UE - FConfigFile
// 섹션과 키는 대소문자를 구분하고, 같은 키가 다시 나오면 뒤의 값을 쓴다.
// 값이 없거나 형식이 맞지 않으면 Get 함수는 Default를 돌려준다.
class FConfigFile
{
public:
	bool Load(const FString& Path);

	FString GetString(const FString& Section, const FString& Key, const FString& Default) const;
	int32 GetInt(const FString& Section, const FString& Key, int32 Default) const;
	// 1/0, true/false를 받는다.
	bool GetBool(const FString& Section, const FString& Key, bool Default) const;

private:
	const FString* Find(const FString& Section, const FString& Key) const;

	// 섹션 이름 → (키 → 값). 첫 [Section] 앞에 있는 줄은 이름이 빈 섹션에 들어간다.
	TMap<FString, TMap<FString, FString>> Sections;
};