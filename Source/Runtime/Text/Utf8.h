#pragma once

#include "Core/EngineString.h"
#include "Containers/Array.h"

// UTF-8 문자열을 유니코드 코드 포인트 배열로 바꾼다. (TextRenderer.cpp에서 옮김)
inline TArray<uint32> DecodeUTF8(const FString& Str)
{
	TArray<uint32> Out;
	const unsigned char* P = reinterpret_cast<const unsigned char*>(Str.c_str());
	const unsigned char* End = P + Str.size();

	while (P < End)
	{
		uint32 CP = 0;
		int Len = 0;
		if (*P < 0x80) { CP = *P;        Len = 1; }
		else if ((*P & 0xE0) == 0xC0) { CP = *P & 0x1F; Len = 2; }
		else if ((*P & 0xF0) == 0xE0) { CP = *P & 0x0F; Len = 3; }
		else if ((*P & 0xF8) == 0xF0) { CP = *P & 0x07; Len = 4; }
		else { ++P; continue; }                         // 잘못된 바이트 스킵

		if (P + Len > End) break;
		for (int i = 1; i < Len; ++i) CP = (CP << 6) | (P[i] & 0x3F);

		Out.Add(CP);
		P += Len;
	}
	return Out;
}