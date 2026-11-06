#include "HeaderParser.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace
{
    // UPROPERTY 지정자 → EPropertyFlags (Runtime/UObject/Property.h)
    struct FFlagSpecifier
    {
        const char* Name;
        const char* Flag;
    };

    constexpr FFlagSpecifier PropertyFlagSpecifiers[] =
    {
        { "EditAnywhere",    "CPF_EditAnywhere" },
        { "VisibleAnywhere", "CPF_VisibleAnywhere" },
        { "Transient",       "CPF_Transient" },
    };

    // UFUNCTION 지정자 → EFunctionFlags (Runtime/UObject/Function.h)
    constexpr FFlagSpecifier FunctionFlagSpecifiers[] =
    {
        { "CallInEditor",   "FUNC_CallInEditor" },
        { "ScriptCallable", "FUNC_ScriptCallable" },
    };

    // UPROPERTY(Type=Color) 에 쓸 수 있는 EPropertyType 이름
    constexpr const char* PropertyTypeNames[] =
    {
        "Float", "Int", "String", "Bool", "Vector", "Vector4", "Color", "LinearColor", "Rotator", "Transform", "Object",
    };

    // UPROPERTY(meta=(...)) 키. 순서는 Runtime/UObject/Property.h의 FPropertyMeta 필드 순서와 같아야 한다.
    // 생성 코드가 C++20 지정 초기화(FPropertyMeta{ .ClampMin = ..., .Delta = ... })를 쓰기 때문이다.
    constexpr const char* PropertyMetaKeys[] = { "ClampMin", "ClampMax", "UIMin", "UIMax", "Delta" };
    constexpr int PropertyMetaKeyCount = (int)std::size(PropertyMetaKeys);

    // meta를 무시하는 위젯(색상 선택기, 체크박스 등)을 쓰는 Type=
    constexpr const char* NonNumericTypeNames[] = { "String", "Bool", "Color", "LinearColor", "Transform", "Object" };

    std::string Quote(const std::string& Text) { return "'" + Text + "'"; }

    // "0.5", "1", "-10", "0.25f" → double. 끝까지 숫자가 아니면 false
    bool ParseNumber(std::string Text, double& OutValue)
    {
        if (!Text.empty() && (Text.back() == 'f' || Text.back() == 'F'))
        {
            Text.pop_back();
        }
        if (Text.empty())
        {
            return false;
        }
        char* End = nullptr;
        OutValue = std::strtod(Text.c_str(), &End);
        return End == Text.c_str() + Text.size() && std::isfinite(OutValue);
    }

    // 0.1 → "0.1f", 1 → "1.0f". float로 표현할 수 있는 가장 짧은 표기를 쓴다.
    std::string MakeFloatLiteral(double Value)
    {
        char Buffer[64];
        const auto Result = std::to_chars(Buffer, Buffer + sizeof(Buffer), (float)Value);
        std::string Text(Buffer, Result.ptr);
        if (Text.find_first_of(".eE") == std::string::npos)
        {
            Text += ".0";
        }
        return Text + "f";
    }
}

FHeaderParser::FHeaderParser(const std::vector<FToken>& InTokens, const fs::path& InFile, FDiagnostics& InDiag)
    : Tokens(InTokens), File(InFile), Diag(InDiag)
{
}

bool FHeaderParser::Parse(std::vector<FClassInfo>& OutClasses, std::vector<FEnumInfo>& OutEnums)
{
    Classes = &OutClasses;
    Enums = &OutEnums;
    Index = 0;
    Scopes.clear();
    FunctionNameCounts.clear();

    while (!AtEnd())
    {
        const FToken& T = Tok();
        if (T.Is("UCLASS"))
        {
            ParseClass();
        }
        else if (T.Is("GENERATED_BODY"))
        {
            ParseGeneratedBody();
        }
        else if (T.Is("UPROPERTY"))
        {
            ParseProperty();
        }
        else if (T.Is("UFUNCTION"))
        {
            ParseFunction();
        }
        else if (T.Is("UENUM"))
        {
            ParseEnum();
        }
        else if (T.Type == ETokenType::Identifier && CurrentClassIndex() >= 0 && Index + 1 < Tokens.size() && Tokens[Index + 1].Is("("))
        {
            // 클래스 본문에서 '이름(' 은 함수 선언이다. UFUNCTION과 같은 이름이 또 있으면 오버로드로 본다.
            ++FunctionNameCounts[CurrentClassIndex()][T.Text];
            ++Index;
        }
        else if (T.Is("{"))
        {
            Scopes.push_back({});
            ++Index;
        }
        else if (T.Is("}"))
        {
            if (Scopes.empty())
            {
                Error(T.Line, "More '}' than '{'.");
                return false;
            }
            if (Scopes.back().ClassIndex >= 0)
            {
                FinishClass(Scopes.back().ClassIndex);
            }
            Scopes.pop_back();
            ++Index;
        }
        else
        {
            ++Index;
        }
    }

    if (!Scopes.empty())
    {
        Error(CurrentLine(), "Unclosed '{'.");
    }
    return !bHasError;
}

int FHeaderParser::CurrentLine() const
{
    if (!AtEnd())
    {
        return Tok().Line;
    }
    return Tokens.empty() ? 1 : Tokens.back().Line;
}

int FHeaderParser::CurrentClassIndex() const
{
    return Scopes.empty() ? -1 : Scopes.back().ClassIndex;
}

bool FHeaderParser::IsInsideAnyClass() const
{
    return std::any_of(Scopes.begin(), Scopes.end(), [](const FScope& Scope) { return Scope.ClassIndex >= 0; });
}

// UCLASS(지정자) class [API매크로] 이름 [final] : [public] 부모 [, 다른 부모] {
void FHeaderParser::ParseClass()
{
    const int Line = Tok().Line;
    ++Index;

    std::vector<FSpecifier> Specifiers;
    if (!ParseSpecifiers("UCLASS", Specifiers))
    {
        return;
    }
    if (IsInsideAnyClass())
    {
        Error(Line, "UCLASS inside a class (nested class) is not supported.");
        return;
    }
    if (AtEnd() || !Tok().Is("class"))
    {
        Error(CurrentLine(), "UCLASS() must be followed by a class definition.");
        return;
    }
    ++Index;

    // 이름: ':' 이나 '{' 앞의 마지막 식별자. ENGINE_API 같은 매크로나 final은 건너뛴다.
    std::string Name;
    while (!AtEnd() && !Tok().Is(":") && !Tok().Is("{") && !Tok().Is(";"))
    {
        if (Tok().Is("("))
        {
            if (!SkipBalanced("(", ")"))
            {
                return;
            }
            continue;
        }
        if (Tok().Type == ETokenType::Identifier && !Tok().Is("final"))
        {
            Name = Tok().Text;
        }
        ++Index;
    }
    if (AtEnd() || Tok().Is(";"))
    {
        Error(Line, "No class definition after UCLASS(). Do not put UCLASS on a forward declaration.");
        return;
    }
    if (Name.empty())
    {
        Error(Line, "Could not find the name of the UCLASS() class.");
        return;
    }
    if (!Tok().Is(":"))
    {
        Error(Line, Quote(Name) + " has no parent class. A UCLASS class must derive from UObject.");
        return;
    }
    ++Index;

    while (!AtEnd() && (Tok().Is("public") || Tok().Is("protected") || Tok().Is("private") || Tok().Is("virtual")))
    {
        ++Index;
    }

    // 첫 번째 부모가 Super. ns::UBase, UBase<T> 처럼 여러 토큰일 수 있다.
    std::string SuperName;
    int AngleDepth = 0;
    while (!AtEnd() && !(AngleDepth == 0 && (Tok().Is(",") || Tok().Is("{"))))
    {
        const FToken& T = Tok();
        if (T.Is("<")) ++AngleDepth;
        else if (T.Is(">")) --AngleDepth;

        const bool bNeedSpace = !SuperName.empty() && (std::isalnum((unsigned char)SuperName.back()) || SuperName.back() == '_')
            && (T.Type == ETokenType::Identifier || T.Type == ETokenType::Number);
        SuperName += (bNeedSpace ? " " : "") + T.Text;
        ++Index;
    }

    // 다중 상속이면 나머지 부모는 무시한다.
    while (!AtEnd() && !Tok().Is("{"))
    {
        ++Index;
    }
    if (AtEnd())
    {
        Error(Line, Quote(Name) + ": could not find the class body '{'.");
        return;
    }
    if (SuperName.empty())
    {
        Error(Line, Quote(Name) + ": could not find the parent class name.");
        return;
    }

    ResolveClassSpecifiers(Specifiers);

    FClassInfo Info;
    Info.Name = Name;
    Info.SuperName = SuperName;
    Info.Line = Line;
    Classes->push_back(std::move(Info));
    FunctionNameCounts.resize(Classes->size());

    Scopes.push_back({ (int)Classes->size() - 1 });
    ++Index; // {
}

void FHeaderParser::ParseGeneratedBody()
{
    const int Line = Tok().Line;
    ++Index;

    if (AtEnd() || !Tok().Is("("))
    {
        Error(Line, "GENERATED_BODY must be followed by ().");
        return;
    }
    if (!SkipBalanced("(", ")"))
    {
        return;
    }

    const int ClassIndex = CurrentClassIndex();
    if (ClassIndex < 0)
    {
        Error(Line, "GENERATED_BODY() must be directly inside a UCLASS() class body.");
        return;
    }

    FClassInfo& Class = (*Classes)[ClassIndex];
    if (Class.GeneratedBodyLine != 0)
    {
        Error(Line, Quote(Class.Name) + " has GENERATED_BODY() twice.");
        return;
    }
    Class.GeneratedBodyLine = Line;
}

// UPROPERTY(지정자) 타입 이름 [= 기본값 | { 기본값 }] ;
// 타입은 해석하지 않는다. 깊이 0에서 = ; { [ : 가 나오기 직전의 식별자가 이름이다.
void FHeaderParser::ParseProperty()
{
    const int Line = Tok().Line;
    ++Index;

    std::vector<FSpecifier> Specifiers;
    if (!ParseSpecifiers("UPROPERTY", Specifiers))
    {
        return;
    }

    const int ClassIndex = CurrentClassIndex();
    if (ClassIndex < 0)
    {
        Error(Line, "UPROPERTY is only allowed on member variables directly inside a UCLASS() class body.");
        return;
    }

    FPropertyInfo Property;
    Property.Line = Line;

    int AngleDepth = 0;
    int ParenDepth = 0; // TFunction<void(int)> 처럼 꺾쇠 안의 괄호
    while (!AtEnd())
    {
        const FToken& T = Tok();
        if (AngleDepth == 0 && ParenDepth == 0)
        {
            if (T.Is("=") || T.Is(";") || T.Is("{") || T.Is("[") || T.Is(":"))
            {
                break;
            }
            if (T.Is("("))
            {
                Error(T.Line, "UPROPERTY is for member variables, not functions. Use UFUNCTION.");
                return;
            }
            if (T.Is(","))
            {
                Error(T.Line, "Declare only one variable per UPROPERTY.");
                return;
            }
            if (T.Is("static") || T.Is("constexpr"))
            {
                Error(T.Line, "UPROPERTY is not allowed on static members (no offsetof).");
                return;
            }
            if (T.Is("}") || T.Is("UPROPERTY") || T.Is("UFUNCTION") || T.Is("GENERATED_BODY"))
            {
                Error(Line, "The declaration after UPROPERTY does not end with ';'.");
                return;
            }
            if (T.Type == ETokenType::Identifier)
            {
                Property.Name = T.Text;
            }
        }

        if (T.Is("<")) ++AngleDepth;
        else if (T.Is(">")) --AngleDepth;
        else if (T.Is("(")) ++ParenDepth;
        else if (T.Is(")")) --ParenDepth;
        ++Index;
    }

    if (AtEnd() || Property.Name.empty())
    {
        Error(Line, "Could not find a member variable name after UPROPERTY.");
        return;
    }

    FClassInfo& Class = (*Classes)[ClassIndex];
    const bool bDuplicate = std::any_of(Class.Properties.begin(), Class.Properties.end(),
        [&](const FPropertyInfo& Other) { return Other.Name == Property.Name; });
    if (bDuplicate)
    {
        Error(Line, Quote(Property.Name) + ": property already declared.");
        return;
    }

    ResolvePropertySpecifiers(Specifiers, Property);
    Class.Properties.push_back(std::move(Property));
}

// UFUNCTION(지정자) [virtual] 반환타입 이름 ( 인자 ) [const] [override] { 본문 } 또는 ;
// UPROPERTY처럼 타입은 해석하지 않는다. 깊이 0의 '(' 직전 식별자가 이름이다.
// 시그니처는 생성 코드의 &ThisClass::이름 에서 컴파일러가 알아낸다.
void FHeaderParser::ParseFunction()
{
    const int Line = Tok().Line;
    ++Index;

    std::vector<FSpecifier> Specifiers;
    if (!ParseSpecifiers("UFUNCTION", Specifiers))
    {
        return;
    }

    const int ClassIndex = CurrentClassIndex();
    if (ClassIndex < 0)
    {
        Error(Line, "UFUNCTION is only allowed on member functions directly inside a UCLASS() class body.");
        return;
    }

    FFunctionInfo Function;
    Function.Line = Line;

    int AngleDepth = 0;
    while (!AtEnd())
    {
        const FToken& T = Tok();
        if (AngleDepth == 0)
        {
            if (T.Is("("))
            {
                break;
            }
            if (T.Is(";") || T.Is("=") || T.Is("{") || T.Is("}") || T.Is("UPROPERTY") || T.Is("UFUNCTION"))
            {
                Error(Line, "No function declaration after UFUNCTION. Use UPROPERTY for member variables.");
                return;
            }
            if (T.Is("static"))
            {
                Error(T.Line, "UFUNCTION is not allowed on static functions (it needs an object to call on).");
                return;
            }
            if (T.Is("template"))
            {
                Error(T.Line, "UFUNCTION is not allowed on template functions.");
                return;
            }
            if (T.Is("operator") || T.Is("~"))
            {
                Error(T.Line, "UFUNCTION is not allowed on operators or destructors.");
                return;
            }
            if (T.Type == ETokenType::Identifier)
            {
                Function.Name = T.Text;
            }
        }

        if (T.Is("<")) ++AngleDepth;
        else if (T.Is(">")) --AngleDepth;
        ++Index;
    }

    FClassInfo& Class = (*Classes)[ClassIndex];
    if (AtEnd() || Function.Name.empty())
    {
        Error(Line, "Could not find a function name after UFUNCTION.");
        return;
    }
    if (Function.Name == Class.Name)
    {
        Error(Line, "UFUNCTION is not allowed on constructors.");
        return;
    }

    // ( ) 또는 ( void ) 이면 인자가 없다.
    const auto TokenIs = [&](size_t At, const char* Text) { return At < Tokens.size() && Tokens[At].Is(Text); };
    const bool bHasNoParams = TokenIs(Index + 1, ")") || (TokenIs(Index + 1, "void") && TokenIs(Index + 2, ")"));

    // 인자 목록은 건너뛴다. 기본 인자 안의 '이름(' 을 함수 선언으로 세지 않도록.
    if (!SkipBalanced("(", ")"))
    {
        return;
    }

    ResolveFunctionSpecifiers(Specifiers, Function);
    const bool bCallInEditor = std::find(Function.Flags.begin(), Function.Flags.end(), "FUNC_CallInEditor") != Function.Flags.end();
    if (bCallInEditor && !bHasNoParams)
    {
        Error(Line, "CallInEditor functions must take no parameters (a details panel button cannot pass arguments).");
    }

    ++FunctionNameCounts[ClassIndex][Function.Name];
    Class.Functions.push_back(std::move(Function));
}

// UENUM(지정자) enum class 이름 [: 기반 타입] { 항목 [= 값] [UMETA(Hidden)], ... };
// 항목 값은 해석하지 않는다. 생성 코드가 static_cast<int64>(E::항목)으로 컴파일러에게 묻는다.
void FHeaderParser::ParseEnum()
{
    const int Line = Tok().Line;
    ++Index;

    std::vector<FSpecifier> Specifiers;
    if (!ParseSpecifiers("UENUM", Specifiers))
    {
        return;
    }
    for (const FSpecifier& Specifier : Specifiers)
    {
        Error(Specifier.Line, "UENUM specifier " + Quote(Specifier.Key) + " is not supported yet.");
    }

    // generated.h가 이 enum을 앞에서 선언(enum class E;)해야 하므로, 앞선 선언이 가능한 곳만 받는다.
    if (!Scopes.empty())
    {
        Error(Line, "UENUM is only allowed at global scope. An enum inside a class or namespace cannot be forward-declared.");
        return;
    }
    if (AtEnd() || !Tok().Is("enum"))
    {
        Error(CurrentLine(), "UENUM() must be followed by an enum class definition.");
        return;
    }
    ++Index;
    if (AtEnd() || !(Tok().Is("class") || Tok().Is("struct")))
    {
        Error(Line, "UENUM supports enum class only. A plain enum cannot be forward-declared without an underlying type.");
        return;
    }
    ++Index;
    if (AtEnd() || Tok().Type != ETokenType::Identifier)
    {
        Error(Line, "Could not find the name of the UENUM() enum.");
        return;
    }

    FEnumInfo Enum;
    Enum.Name = Tok().Text;
    Enum.Line = Line;
    ++Index;

    // : uint8 같은 기반 타입. 앞선 선언에도 똑같이 써야 한다.
    if (!AtEnd() && Tok().Is(":"))
    {
        ++Index;
        while (!AtEnd() && !Tok().Is("{") && !Tok().Is(";"))
        {
            const bool bNeedSpace = !Enum.UnderlyingType.empty() && Tok().Type == ETokenType::Identifier
                && (std::isalnum((unsigned char)Enum.UnderlyingType.back()) || Enum.UnderlyingType.back() == '_');
            Enum.UnderlyingType += (bNeedSpace ? " " : "") + Tok().Text;
            ++Index;
        }
    }
    if (AtEnd() || Tok().Is(";"))
    {
        Error(Line, "No enum definition after UENUM(). Do not put UENUM on a forward declaration.");
        return;
    }
    ++Index; // {

    while (!AtEnd() && !Tok().Is("}"))
    {
        if (Tok().Type != ETokenType::Identifier)
        {
            Error(Tok().Line, "Expected an enum entry name but found " + Quote(Tok().Text) + ".");
            return;
        }

        FEnumEntryInfo Entry;
        Entry.Name = Tok().Text;
        const int EntryLine = Tok().Line;
        ++Index;

        // = 값 부분은 건너뛴다.
        int ParenDepth = 0;
        while (!AtEnd() && !(ParenDepth == 0 && (Tok().Is(",") || Tok().Is("}") || Tok().Is("UMETA"))))
        {
            if (Tok().Is("(")) ++ParenDepth;
            else if (Tok().Is(")")) --ParenDepth;
            ++Index;
        }

        if (!AtEnd() && Tok().Is("UMETA"))
        {
            ++Index;
            std::vector<FSpecifier> EntryMeta;
            if (!ParseSpecifiers("UMETA", EntryMeta))
            {
                return;
            }
            for (const FSpecifier& Meta : EntryMeta)
            {
                if (Meta.Key == "Hidden")
                {
                    Entry.bHidden = true;
                }
                else
                {
                    Error(Meta.Line, "Unknown UMETA specifier " + Quote(Meta.Key) + ". Only Hidden is supported.");
                }
            }
        }

        const bool bDuplicate = std::any_of(Enum.Entries.begin(), Enum.Entries.end(),
            [&](const FEnumEntryInfo& Other) { return Other.Name == Entry.Name; });
        if (bDuplicate)
        {
            Error(EntryLine, "Duplicate enum entry " + Quote(Entry.Name) + ".");
        }
        Enum.Entries.push_back(std::move(Entry));

        if (!AtEnd() && Tok().Is(","))
        {
            ++Index;
        }
    }
    if (AtEnd())
    {
        Error(Line, Quote(Enum.Name) + ": could not find the closing '}'.");
        return;
    }
    ++Index; // }

    if (Enum.Entries.empty())
    {
        Error(Line, Quote(Enum.Name) + " has no entries.");
        return;
    }
    Enums->push_back(std::move(Enum));
}

void FHeaderParser::FinishClass(int ClassIndex)
{
    const FClassInfo& Class = (*Classes)[ClassIndex];
    if (Class.GeneratedBodyLine == 0)
    {
        Error(Class.Line, Quote(Class.Name) + " has no GENERATED_BODY().");
    }

    // &ThisClass::이름 은 같은 이름의 함수가 여럿이면 어느 것인지 정할 수 없다. UE도 UFUNCTION 오버로드를 막는다.
    for (const FFunctionInfo& Function : Class.Functions)
    {
        const auto It = FunctionNameCounts[ClassIndex].find(Function.Name);
        if (It != FunctionNameCounts[ClassIndex].end() && It->second > 1)
        {
            Error(Function.Line, Quote(Function.Name) + ": another function with the same name exists in this class. UFUNCTION cannot be overloaded.");
        }
    }
}

// ( 이름 [= 값] , 이름 [= 값] ... )
bool FHeaderParser::ParseSpecifiers(const char* MacroName, std::vector<FSpecifier>& OutSpecifiers)
{
    if (!Expect("(", MacroName))
    {
        return false;
    }

    while (!AtEnd() && !Tok().Is(")"))
    {
        if (Tok().Type != ETokenType::Identifier)
        {
            Error(Tok().Line, std::string(MacroName) + "(...): expected a specifier name but found " + Quote(Tok().Text) + ".");
            return false;
        }

        FSpecifier Specifier;
        Specifier.Key = Tok().Text;
        Specifier.Line = Tok().Line;
        ++Index;

        if (!AtEnd() && Tok().Is("="))
        {
            ++Index;
            if (AtEnd())
            {
                break;
            }
            if (Tok().Is("("))
            {
                // meta=(ClampMin=0, Delta=0.1) 같은 하위 목록
                if (!ParseSpecifiers(Specifier.Key.c_str(), Specifier.Children))
                {
                    return false;
                }
            }
            else
            {
                // -10 은 '-' 와 '10' 두 토큰이다.
                if ((Tok().Is("-") || Tok().Is("+")) && Index + 1 < Tokens.size() && Tokens[Index + 1].Type == ETokenType::Number)
                {
                    Specifier.Value = Tok().Text;
                    ++Index;
                }
                Specifier.Value += Tok().Text;
                if (Tok().Type == ETokenType::String && Specifier.Value.size() >= 2)
                {
                    Specifier.Value = Specifier.Value.substr(1, Specifier.Value.size() - 2);
                }
                ++Index;
            }
        }
        OutSpecifiers.push_back(std::move(Specifier));

        if (!AtEnd() && Tok().Is(","))
        {
            ++Index;
        }
        else if (!AtEnd() && !Tok().Is(")"))
        {
            Error(Tok().Line, std::string(MacroName) + "(...): expected ',' or ')' but found " + Quote(Tok().Text) + ".");
            return false;
        }
    }
    return Expect(")", MacroName);
}

// Tok()가 Open일 때 짝이 맞는 Close까지 넘긴다. OutText에는 안쪽 토큰을 공백으로 이어 담는다.
bool FHeaderParser::SkipBalanced(const char* Open, const char* Close, std::string* OutText)
{
    const int Line = Tok().Line;
    int Depth = 0;
    while (!AtEnd())
    {
        const FToken& T = Tok();
        if (T.Is(Open))
        {
            ++Depth;
        }
        else if (T.Is(Close))
        {
            --Depth;
            if (Depth == 0)
            {
                ++Index;
                return true;
            }
        }

        if (OutText && Depth > 0 && !(Depth == 1 && T.Is(Open)))
        {
            *OutText += (OutText->empty() ? "" : " ") + T.Text;
        }
        ++Index;
    }
    Error(Line, "Unclosed " + Quote(Open) + ".");
    return false;
}

bool FHeaderParser::Expect(const char* Text, const char* Context)
{
    if (AtEnd() || !Tok().Is(Text))
    {
        const std::string Found = AtEnd() ? "end of file" : Quote(Tok().Text);
        Error(CurrentLine(), std::string(Context) + ": expected " + Quote(Text) + " but found " + Found + ".");
        return false;
    }
    ++Index;
    return true;
}

void FHeaderParser::ResolveClassSpecifiers(const std::vector<FSpecifier>& Specifiers)
{
    // 아직 UCLASS 지정자는 없다. 추가할 때 여기에 표를 만든다.
    for (const FSpecifier& Specifier : Specifiers)
    {
        Error(Specifier.Line, "UCLASS specifier " + Quote(Specifier.Key) + " is not supported yet.");
    }
}

void FHeaderParser::ResolvePropertySpecifiers(const std::vector<FSpecifier>& Specifiers, FPropertyInfo& Property)
{
    bool bHasEditSpecifier = false;
    for (const FSpecifier& Specifier : Specifiers)
    {
        const auto FlagIt = std::find_if(std::begin(PropertyFlagSpecifiers), std::end(PropertyFlagSpecifiers),
            [&](const FFlagSpecifier& Entry) { return Specifier.Key == Entry.Name; });

        if (FlagIt != std::end(PropertyFlagSpecifiers))
        {
            const bool bIsEditSpecifier = Specifier.Key == "EditAnywhere" || Specifier.Key == "VisibleAnywhere";
            if (bIsEditSpecifier && bHasEditSpecifier)
            {
                Error(Specifier.Line, "Use only one of EditAnywhere and VisibleAnywhere.");
                continue;
            }
            bHasEditSpecifier |= bIsEditSpecifier;

            if (std::find(Property.Flags.begin(), Property.Flags.end(), FlagIt->Flag) != Property.Flags.end())
            {
                Error(Specifier.Line, "Duplicate specifier " + Quote(Specifier.Key) + ".");
                continue;
            }
            Property.Flags.push_back(FlagIt->Flag);
        }
        else if (Specifier.Key == "Type")
        {
            const bool bValidType = std::any_of(std::begin(PropertyTypeNames), std::end(PropertyTypeNames),
                [&](const char* Name) { return Specifier.Value == Name; });
            if (!bValidType)
            {
                Error(Specifier.Line, "Type=" + Specifier.Value + ": not an EPropertyType name.");
                continue;
            }
            Property.TypeOverride = Specifier.Value;
        }
        else if (Specifier.Key == "meta")
        {
            ResolvePropertyMeta(Specifier, Property);
        }
        else
        {
            Error(Specifier.Line, "Unknown UPROPERTY specifier " + Quote(Specifier.Key) + ".");
        }
    }

    const bool bNonNumericType = std::any_of(std::begin(NonNumericTypeNames), std::end(NonNumericTypeNames),
        [&](const char* Name) { return Property.TypeOverride == Name; });
    if (!Property.Meta.empty() && bNonNumericType)
    {
        Error(Property.Line, "Type=" + Property.TypeOverride + ": this widget does not use meta range/step.");
    }
}

void FHeaderParser::ResolveFunctionSpecifiers(const std::vector<FSpecifier>& Specifiers, FFunctionInfo& Function)
{
    for (const FSpecifier& Specifier : Specifiers)
    {
        const auto FlagIt = std::find_if(std::begin(FunctionFlagSpecifiers), std::end(FunctionFlagSpecifiers),
            [&](const FFlagSpecifier& Entry) { return Specifier.Key == Entry.Name; });
        if (FlagIt == std::end(FunctionFlagSpecifiers))
        {
            Error(Specifier.Line, "Unknown UFUNCTION specifier " + Quote(Specifier.Key) + ". Use CallInEditor or ScriptCallable.");
            continue;
        }
        if (std::find(Function.Flags.begin(), Function.Flags.end(), FlagIt->Flag) != Function.Flags.end())
        {
            Error(Specifier.Line, "Duplicate specifier " + Quote(Specifier.Key) + ".");
            continue;
        }
        Function.Flags.push_back(FlagIt->Flag);
    }
}

// meta=(ClampMin=0, ClampMax=1, UIMin=0, UIMax=1, Delta=0.01)
void FHeaderParser::ResolvePropertyMeta(const FSpecifier& MetaSpecifier, FPropertyInfo& Property)
{
    if (MetaSpecifier.Children.empty())
    {
        Error(MetaSpecifier.Line, "Write meta with parentheses, e.g. meta=(ClampMin=0, Delta=0.1).");
        return;
    }

    double Values[PropertyMetaKeyCount] = {};
    bool bIsSet[PropertyMetaKeyCount] = {};

    for (const FSpecifier& Entry : MetaSpecifier.Children)
    {
        const auto KeyIt = std::find_if(std::begin(PropertyMetaKeys), std::end(PropertyMetaKeys),
            [&](const char* Key) { return Entry.Key == Key; });
        if (KeyIt == std::end(PropertyMetaKeys))
        {
            Error(Entry.Line, "Unknown meta key " + Quote(Entry.Key) + ". Use one of ClampMin, ClampMax, UIMin, UIMax, Delta.");
            continue;
        }

        const int KeyIndex = (int)(KeyIt - std::begin(PropertyMetaKeys));
        if (bIsSet[KeyIndex])
        {
            Error(Entry.Line, "Duplicate meta " + Entry.Key + ".");
            continue;
        }
        if (!ParseNumber(Entry.Value, Values[KeyIndex]))
        {
            Error(Entry.Line, "meta " + Entry.Key + ": value " + Quote(Entry.Value) + " is not a number.");
            continue;
        }
        bIsSet[KeyIndex] = true;
    }

    enum { ClampMin, ClampMax, UIMin, UIMax, Delta };
    if (bIsSet[Delta] && Values[Delta] <= 0.0)
    {
        Error(MetaSpecifier.Line, "meta Delta must be greater than 0.");
    }
    if (bIsSet[ClampMin] && bIsSet[ClampMax] && Values[ClampMin] > Values[ClampMax])
    {
        Error(MetaSpecifier.Line, "meta ClampMin is greater than ClampMax.");
    }
    if (bIsSet[UIMin] && bIsSet[UIMax] && Values[UIMin] > Values[UIMax])
    {
        Error(MetaSpecifier.Line, "meta UIMin is greater than UIMax.");
    }

    for (int KeyIndex = 0; KeyIndex < PropertyMetaKeyCount; ++KeyIndex)
    {
        if (bIsSet[KeyIndex])
        {
            Property.Meta.push_back({ PropertyMetaKeys[KeyIndex], MakeFloatLiteral(Values[KeyIndex]) });
        }
    }
}

void FHeaderParser::Error(int Line, const std::string& Message)
{
    bHasError = true;
    Diag.Error(File, Line, Message);
}