#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include <cstdio>
#include <random>

// UE FGuid 축소판. 시퀀스 바인딩 ID로 쓴다.
struct FGuid
{
	uint32 A = 0;
	uint32 B = 0;
	uint32 C = 0;
	uint32 D = 0;

	static FGuid NewGuid()
	{
		static std::mt19937 Generator{ std::random_device{}() };
		std::uniform_int_distribution<uint32> Distribution;
		FGuid Result;
		do
		{
			Result = { Distribution(Generator), Distribution(Generator), Distribution(Generator), Distribution(Generator) };
		} while (!Result.IsValid());
		return Result;
	}

	bool IsValid() const { return (A | B | C | D) != 0; }
	void Invalidate() { A = B = C = D = 0; }

	bool operator==(const FGuid& Other) const { return A == Other.A && B == Other.B && C == Other.C && D == Other.D; }
	bool operator!=(const FGuid& Other) const { return !(*this == Other); }

	// UE EGuidFormats::Digits와 같은 32자리 16진수
	FString ToString() const
	{
		char Buffer[33];
		std::snprintf(Buffer, sizeof(Buffer), "%08X%08X%08X%08X", A, B, C, D);
		return Buffer;
	}

	static bool Parse(const FString& String, FGuid& OutGuid)
	{
		if (String.size() != 32) return false;
		uint32 Parts[4];
		for (int32 Index = 0; Index < 4; ++Index)
		{
			try { Parts[Index] = static_cast<uint32>(std::stoul(String.substr(Index * 8, 8), nullptr, 16)); }
			catch (...) { return false; }
		}
		OutGuid = { Parts[0], Parts[1], Parts[2], Parts[3] };
		return true;
	}
};
