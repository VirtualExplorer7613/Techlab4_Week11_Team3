#pragma once

#include "HhtTypes.h"

// Visual Studio 오류 목록이 읽는 형식으로 출력한다.
//   Source\Runtime\Foo.h(12): error HHT001: 메시지
class FDiagnostics
{
public:
    void Error(const fs::path& File, int Line, const std::string& Message);
    int GetErrorCount() const { return ErrorCount; }

private:
    int ErrorCount = 0;
};

std::string ToUtf8(const fs::path& Path);

// UTF-8 문자열을 콘솔(또는 MSBuild가 읽는 파이프)의 코드 페이지로 바꿔 출력한다.
void PrintLine(const std::string& Utf8Text, bool bError = false);