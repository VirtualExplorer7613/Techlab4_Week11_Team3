#include "Diagnostics.h"

#include <cstdio>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

void FDiagnostics::Error(const fs::path& File, int Line, const std::string& Message)
{
	++ErrorCount;
	PrintLine(ToUtf8(fs::absolute(File).make_preferred()) + "(" + std::to_string(Line) + "): error HHT001: " + Message, true);
}

std::string ToUtf8(const fs::path& Path)
{
	const std::u8string Text = Path.u8string();
	return std::string(Text.begin(), Text.end());
}

void PrintLine(const std::string& Utf8Text, bool bError)
{
	std::string Output = Utf8Text;
	const UINT CodePage = GetConsoleOutputCP();
	if (CodePage != 0 && CodePage != CP_UTF8)
	{
		const int WideLength = MultiByteToWideChar(CP_UTF8, 0, Utf8Text.data(), (int)Utf8Text.size(), nullptr, 0);
		std::wstring Wide(WideLength, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, Utf8Text.data(), (int)Utf8Text.size(), Wide.data(), WideLength);

		const int Length = WideCharToMultiByte(CodePage, 0, Wide.data(), WideLength, nullptr, 0, nullptr, nullptr);
		Output.assign(Length, '\0');
		WideCharToMultiByte(CodePage, 0, Wide.data(), WideLength, Output.data(), Length, nullptr, nullptr);
	}

	FILE* Stream = bError ? stderr : stdout;
	std::fputs(Output.c_str(), Stream);
	std::fputc('\n', Stream);
}