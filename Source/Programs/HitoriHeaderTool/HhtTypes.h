#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

enum class ETokenType
{
	Identifier, 
	Symbol,
	Number,
	String,
	Char
};

struct FToken
{
	ETokenType Type = ETokenType::Symbol;
	std::string Text;
	int Line = 0;

	// 식별자·기호를 글자 그대로 비교한다. 문자열 안의 "UCLASS"는 걸리지 않는다.
	bool Is(const char* InText) const
	{
		return (Type == ETokenType::Identifier || Type == ETokenType::Symbol) && Text == InText;
	}
};

// UCLASS(...) / UPROPERTY(...) 괄호 안의 항목 하나. Key, Key=Value, Key=(하위 목록)
struct FSpecifier
{
	std::string Key;
	std::string Value;
	std::vector<FSpecifier> Children;
	int Line = 0;
};

// meta 키 하나와 생성 코드에 들어갈 float 리터럴. 예: { "ClampMin", "0.0f" }
struct FMetaValue
{
	std::string Key;
	std::string Literal;
};

struct FPropertyInfo
{
	std::string Name;
	int Line = 0;

	// 지정자를 해석한 결과. 코드 생성기는 이 값만 본다.
	std::vector<std::string> Flags;   // CPF_EditAnywhere 등
	std::string TypeOverride;         // Type=Color → "Color"
	std::vector<FMetaValue> Meta;     // FPropertyMeta 필드 순서대로 정렬돼 있다
};

struct FFunctionInfo
{
	std::string Name;
	int Line = 0;
	std::vector<std::string> Flags;   // FUNC_CallInEditor 등
};

struct FClassInfo
{
	std::string Name;
	std::string SuperName;
	int Line = 0;
	int GeneratedBodyLine = 0;
	std::vector<FPropertyInfo> Properties;
	std::vector<FFunctionInfo> Functions;
};

struct FEnumEntryInfo
{
	std::string Name;
	bool bHidden = false; // UMETA(Hidden)
};

struct FEnumInfo
{
	std::string Name;
	std::string UnderlyingType; // enum class E : uint8 → "uint8". 없으면 빈 문자열(int)
	int Line = 0;
	std::vector<FEnumEntryInfo> Entries;
};

struct FHeaderInfo
{
	fs::path Path;
	std::string RelativePath;    // Runtime/Components/BoxComponent.h
	std::string FileId;         // FID_Runtime_Components_BoxComponent_h
	std::string GeneratedName;   // BoxComponent.generated.h
	std::vector<FClassInfo> Classes;
	std::vector<FEnumInfo> Enums;

	// UCLASS / UENUM 중 파일에서 처음 나오는 줄. .generated.h는 이보다 앞에 include해야 한다.
	int GetFirstDeclarationLine() const
	{
		int Line = 0;
		for (const FClassInfo& Class : Classes) Line = (Line == 0 || Class.Line < Line) ? Class.Line : Line;
		for (const FEnumInfo& Enum : Enums) Line = (Line == 0 || Enum.Line < Line) ? Enum.Line : Line;
		return Line;
	}
};
