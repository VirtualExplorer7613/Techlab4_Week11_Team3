#pragma once

#include "Diagnostics.h"

#include <map>

class FHeaderParser
{
public:
	FHeaderParser(const std::vector<FToken>& InTokens, const fs::path& InFile, FDiagnostics& InDaig);

	bool Parse(std::vector<FClassInfo>& OutClasses, std::vector<FEnumInfo>& OutEnums);

private:
	struct FScope
	{
		int ClassIndex = -1;
	};

	bool AtEnd() const { return Index >= Tokens.size(); }
	const FToken& Tok() const { return Tokens[Index]; }
	int CurrentLine() const;

	int CurrentClassIndex() const;
	bool IsInsideAnyClass() const;

	void ParseClass();
	void ParseGeneratedBody();
	void ParseProperty();
	void ParseFunction();
	void ParseEnum();
	void FinishClass(int ClassIndex);

	bool ParseSpecifiers(const char* MacroName, std::vector<FSpecifier>& OutSpecifiers);
	bool SkipBalanced(const char* Open, const char* Close, std::string* OutText = nullptr);
	bool Expect(const char* Text, const char* Context);

	void ResolveClassSpecifiers(const std::vector<FSpecifier>& Specifiers);
	void ResolvePropertySpecifiers(const std::vector<FSpecifier>& Specifiers, FPropertyInfo& Property);
	void ResolvePropertyMeta(const FSpecifier& MetaSpecifier, FPropertyInfo& Property);
	void ResolveFunctionSpecifiers(const std::vector<FSpecifier>& Specifiers, FFunctionInfo& Function);

	void Error(int Line, const std::string& Message);

	const std::vector<FToken>& Tokens;
	const fs::path& File;
	FDiagnostics& Diag;

	size_t Index = 0;
	bool bHasError = false;
	std::vector<FScope> Scopes;
	std::vector<FClassInfo>* Classes = nullptr;
	std::vector<FEnumInfo>* Enums = nullptr;
	std::vector<std::map<std::string, int>> FunctionNameCounts; // 클래스마다 선언된 함수 이름 개수
};