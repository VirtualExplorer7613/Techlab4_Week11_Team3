#include "EnginePCH.h"
#include "ConfigFile.h"

namespace
{
	// 앞뒤 공백과 CRLF 파일의 \r을 지운다.
	FString Trim(const FString& Text)
	{
		const size_t First = Text.find_first_not_of(" \t\r");
		if (First == FString::npos) return {};
		const size_t Last = Text.find_last_not_of(" \t\r");
		return Text.substr(First, Last - First + 1);
	}
}


bool FConfigFile::Load(const FString& Path)
{
	std::ifstream File(Path);
	if (!File) return false;

	Sections.Reset();
	FString Section;
	FString Line;
	bool bFirstLine = true;
	while (std::getline(File, Line))
	{
		// UTF-8 로 저장될 경우 붙여지는 BOM(Byte Order Mark)를 제거한다.
		if (bFirstLine && Line.rfind("\xEF\xBB\xBF", 0) == 0) Line.erase(0, 3);
		bFirstLine = false;

		Line = Trim(Line);
		if (Line.empty() || Line[0] == ';' || Line[0] == '#') continue;

		if (Line.front() == '[' && Line.back() == ']')
		{
			Section = Trim(Line.substr(1, Line.size() - 2));
			continue;
		}

		const size_t Equal = Line.find('=');
		if (Equal == FString::npos) continue;
		// Add는 같은 키가 이미 있으면 값을 덮어쓴다.
		Sections.FindOrAdd(Section).Add(Trim(Line.substr(0, Equal)), Trim(Line.substr(Equal + 1)));
	}
	return true;
}

const FString* FConfigFile::Find(const FString& Section, const FString& Key) const
{
	const TMap<FString, FString>* Keys = Sections.Find(Section);
	return Keys ? Keys->Find(Key) : nullptr;
}

FString FConfigFile::GetString(const FString& Section, const FString& Key, const FString& Default) const
{
	const FString* Value = Find(Section, Key);
	return Value ? *Value : Default;
}

int32 FConfigFile::GetInt(const FString& Section, const FString& Key, int32 Default) const
{
	const FString* Value = Find(Section, Key);
	if (!Value || Value->empty()) return Default;
	int32 Result = 0;
	const char* End = Value->data() + Value->size();
	const auto [Ptr, Error] = std::from_chars(Value->data(), End, Result);
	// "12abc"처럼 뒤에 다른 글자가 붙어 있으면 잘못된 값으로 본다.
	return Error == std::errc() && Ptr == End ? Result : Default;
}

bool FConfigFile::GetBool(const FString& Section, const FString& Key, bool Default) const
{
	const FString* Value = Find(Section, Key);
	if (!Value) return Default;
	if (*Value == "1" || *Value == "true" || *Value == "True") return true;
	if (*Value == "0" || *Value == "false" || *Value == "False") return false;
	return Default;
}
