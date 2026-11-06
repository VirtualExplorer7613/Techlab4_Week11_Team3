// HitoriHeaderTool (HHT)
// 사용법: HitoriHeaderTool <모듈 이름> <출력 폴더> <소스 폴더>...
//   예:   HitoriHeaderTool Runtime Intermediate/Generated/Runtime Source/Runtime
//
// 소스 폴더의 헤더 중 UCLASS()나 UENUM()이 있는 것마다 <출력 폴더>/<헤더 이름>.generated.h를 만든다.
// 오류가 하나라도 있으면 아무 파일도 쓰지 않고 1을 반환해서 빌드를 멈춘다.

#include "CodeGenerator.h"
#include "HeaderParser.h"
#include "Tokenizer.h"

#include <cctype>
#include <map>
#include <set>
#include <sstream>

namespace
{
    // Runtime/Components/BoxComponent.h → Runtime_Components_BoxComponent_h
    std::string MakeIdentifier(const std::string& Text)
    {
        std::string Result = Text;
        for (char& C : Result)
        {
            if (!std::isalnum((unsigned char)C))
            {
                C = '_';
            }
        }
        return Result;
    }

    // .generated.h는 UCLASS/UENUM보다 앞에 있는 #include 중 마지막이어야 한다.
    // 뒤에 다른 헤더가 오면 그 헤더의 generated.h가 CURRENT_FILE_ID를 덮어쓴다.
    void CheckGeneratedInclude(const std::string& Source, const FHeaderInfo& Header, FDiagnostics& Diag)
    {
        const int FirstClassLine = Header.GetFirstDeclarationLine();
        const std::string Expected = "\"" + Header.GeneratedName + "\"";

        int GeneratedLine = 0;
        int LastIncludeLine = 0;

        std::istringstream Stream(Source);
        std::string LineText;
        for (int Line = 1; std::getline(Stream, LineText); ++Line)
        {
            const size_t First = LineText.find_first_not_of(" \t");
            if (First == std::string::npos || LineText[First] != '#')
            {
                continue;
            }
            const size_t Directive = LineText.find_first_not_of(" \t", First + 1);
            if (Directive == std::string::npos || LineText.compare(Directive, 7, "include") != 0)
            {
                continue;
            }

            if (LineText.find(Expected) != std::string::npos)
            {
                GeneratedLine = Line;
            }
            else if (Line < FirstClassLine)
            {
                LastIncludeLine = Line;
            }
        }

        if (GeneratedLine == 0)
        {
            Diag.Error(Header.Path, FirstClassLine, "#include " + Expected + "가 없어요. 헤더의 마지막 #include로 추가하세요.");
        }
        else if (GeneratedLine > FirstClassLine)
        {
            Diag.Error(Header.Path, GeneratedLine, Header.GeneratedName + "는 UCLASS()/UENUM()보다 앞에 include해야 해요.");
        }
        else if (LastIncludeLine > GeneratedLine)
        {
            Diag.Error(Header.Path, LastIncludeLine, Header.GeneratedName + "가 마지막 #include여야 해요. 이 #include를 그 위로 옮기세요.");
        }
    }

    fs::path NormalizeDirectory(const fs::path& Directory)
    {
        fs::path Result = fs::absolute(Directory).lexically_normal();
        if (Result.filename().empty())
        {
            Result = Result.parent_path(); // 끝의 / 제거
        }
        return Result;
    }

    void CollectHeaders(const fs::path& SourceDir, std::vector<FHeaderInfo>& OutHeaders, FDiagnostics& Diag)
    {
        if (!fs::is_directory(SourceDir))
        {
            Diag.Error(SourceDir, 1, "Source Dir not exists.");
            return;
        }

        for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(SourceDir))
        {
            const fs::path& Path = Entry.path();
            if (!Entry.is_regular_file() || (Path.extension() != ".h" && Path.extension() != ".hpp"))
            {
                continue;
            }

            std::string Source;
            if (!ReadTextFile(Path, Source))
            {
                Diag.Error(Path, 1, "Failed to read file.");
                continue;
            }
            // 대부분의 헤더는 여기서 걸러진다.
            if (Source.find("UCLASS") == std::string::npos && Source.find("UENUM") == std::string::npos)
            {
                continue;
            }

            std::vector<FToken> Tokens;
            if (!Tokenize(Source, Path, Tokens, Diag))
            {
                continue;
            }

            FHeaderInfo Header;
            Header.Path = Path;
            FHeaderParser Parser(Tokens, Path, Diag);
            if (!Parser.Parse(Header.Classes, Header.Enums) || (Header.Classes.empty() && Header.Enums.empty()))
            {
                continue;
            }

            // FileId는 소스 폴더의 부모 기준 경로로 만든다. 모듈끼리 겹치지 않게 Runtime/, Editor/가 앞에 붙는다.
            Header.RelativePath = fs::relative(Path, SourceDir.parent_path()).generic_string();
            Header.FileId = "FID_" + MakeIdentifier(Header.RelativePath);
            Header.GeneratedName = ToUtf8(Path.stem()) + ".generated.h";

            CheckGeneratedInclude(Source, Header, Diag);
            OutHeaders.push_back(std::move(Header));
        }
    }

    int Run(int Argc, char** Argv)
    {
        if (Argc < 4)
        {
            PrintLine("How To Use: HitoriHeaderTool <Module Name> <Output Folder> <Source Folder>...", true);
            return 2;
        }

        const std::string ModuleName = Argv[1];
        const fs::path OutputDir = NormalizeDirectory(Argv[2]);

        FDiagnostics Diag;
        std::vector<FHeaderInfo> Headers;
        for (int I = 3; I < Argc; ++I)
        {
            CollectHeaders(NormalizeDirectory(Argv[I]), Headers, Diag);
        }

        // 생성 파일은 한 폴더에 모이므로 헤더 이름이 겹치면 안 된다.
        std::map<std::string, fs::path> GeneratedNames;
        for (const FHeaderInfo& Header : Headers)
        {
            const auto [It, bInserted] = GeneratedNames.emplace(Header.GeneratedName, Header.Path);
            if (!bInserted)
            {
                Diag.Error(Header.Path, 1, Header.GeneratedName + " Name " + ToUtf8(It->second) + " Already Exists. 모듈 안에서 UCLASS/UENUM 헤더 이름은 달라야 해요.");
            }
        }

        if (Diag.GetErrorCount() > 0)
        {
            PrintLine("HHT: " + ModuleName + " - " + std::to_string(Diag.GetErrorCount()) + "Errors. Not Written Generated Files.", true);
            return 1;
        }

        fs::create_directories(OutputDir);

        int WrittenCount = 0;
        for (const FHeaderInfo& Header : Headers)
        {
            bool bWritten = false;
            if (!WriteIfChanged(OutputDir / Header.GeneratedName, GenerateHeader(Header, ModuleName), bWritten, Diag))
            {
                return 1;
            }
            WrittenCount += bWritten ? 1 : 0;
        }

        // UCLASS/UENUM이 사라진 헤더의 생성 파일은 지운다.
        int RemovedCount = 0;
        for (const fs::directory_entry& Entry : fs::directory_iterator(OutputDir))
        {
            const std::string Name = ToUtf8(Entry.path().filename());
            const bool bIsGenerated = Name.size() > 12 && Name.ends_with(".generated.h");
            if (bIsGenerated && !GeneratedNames.contains(Name))
            {
                fs::remove(Entry.path());
                ++RemovedCount;
            }
        }

        PrintLine("HHT: " + ModuleName + " - Reflection Header " + std::to_string(Headers.size()) + " (Written "
            + std::to_string(WrittenCount) + ", Not Written " + std::to_string(Headers.size() - WrittenCount)
            + ", Removed " + std::to_string(RemovedCount) + ")");
        return 0;
    }
}

int main(int Argc, char** Argv)
{
    try
    {
        return Run(Argc, Argv);
    }
    catch (const fs::filesystem_error& Exception)
    {
        PrintLine(std::string("HHT: File System Error: ") + Exception.what(), true);
        return 1;
    }
}