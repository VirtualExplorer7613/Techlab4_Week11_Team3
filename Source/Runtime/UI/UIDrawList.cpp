#include "EnginePCH.h"
#include "UI/UIDrawList.h"

#include "Text/Font.h"
#include "Text/Utf8.h"

void FUIDrawList::Reset()
{
	Vertices.Reset();
	Indices.Reset();
	Batches.Reset();
}

void FUIDrawList::AddQuad(float L, float T, float R, float B, float U0, float V0, float U1, float V1,
	const FLinearColor& Color, EUIDrawMode Mode, UTexture2D* Texture, float PxRange)
{
	// 단색 사각형은 텍스처를 읽지 않으므로 현재 배치에 그대로 붙인다.
	FUIDrawBatch* Batch = Batches.IsEmpty() ? nullptr : &Batches.Last();
	if (Batch && Mode != EUIDrawMode::Solid)
	{
		if (!Batch->Texture)
		{
			Batch->Texture = Texture;
			Batch->PxRange = PxRange;
		}
		else if (Batch->Texture != Texture)
		{
			Batch = nullptr;
		}
	}
	if (!Batch)
	{
		Batches.Add({ Mode == EUIDrawMode::Solid ? nullptr : Texture, PxRange, static_cast<uint32>(Indices.Num()), 0 });
		Batch = &Batches.Last();
	}

	const uint32 Base = static_cast<uint32>(Vertices.Num());
	const float M = static_cast<float>(Mode);
	Vertices.Add({ FVector2(L, T), FVector2(U0, V0), Color, M });
	Vertices.Add({ FVector2(R, T), FVector2(U1, V0), Color, M });
	Vertices.Add({ FVector2(R, B), FVector2(U1, V1), Color, M });
	Vertices.Add({ FVector2(L, B), FVector2(U0, V1), Color, M });

	Indices.Add(Base + 0); Indices.Add(Base + 1); Indices.Add(Base + 2);
	Indices.Add(Base + 0); Indices.Add(Base + 2); Indices.Add(Base + 3);
	Batch->IndexCount += 6;
}

void FUIDrawList::AddRect(const FUIRect& Rect, const FLinearColor& Color)
{
	AddQuad(Rect.X, Rect.Y, Rect.X + Rect.W, Rect.Y + Rect.H, 0, 0, 0, 0, Color, EUIDrawMode::Solid, nullptr, 0.0f);
}

void FUIDrawList::AddImage(const FUIRect& Rect, UTexture2D* Texture, const FLinearColor& Tint)
{
	AddQuad(Rect.X, Rect.Y, Rect.X + Rect.W, Rect.Y + Rect.H, 0, 0, 1, 1, Tint, EUIDrawMode::Texture, Texture, 0.0f);
}

FVector2 FUIDrawList::MeasureText(const FString& Text, const UFont& Font, float FontSize)
{
	float Width = 0.0f;
	uint32 Prev = 0;
	for (uint32 CP : DecodeUTF8(Text))
	{
		const FGlyphInfo* Glyph = Font.GlyphMap.Find(CP);
		if (!Glyph) continue;
		if (Prev)
			if (const float* Kern = Font.KerningMap.Find((static_cast<uint64>(Prev) << 32) | CP))
				Width += *Kern * FontSize;
		Width += Glyph->Advance * FontSize;
		Prev = CP;
	}
	return FVector2(Width, Font.LineHeight * FontSize);
}

void FUIDrawList::AddText(const FUIRect& Rect, const FString& Text, const UFont& Font, float FontSize,
	const FLinearColor& Color, int32 Justify)
{
	if (Text.empty() || !Font.AtlasTexture) return;

	const float Width = MeasureText(Text, Font, FontSize).X;
	float PenX = Rect.X;
	if (Justify == 1) PenX += (Rect.W - Width) * 0.5f;
	else if (Justify == 2) PenX += Rect.W - Width;

	// 대문자 H의 높이로 세로 가운데를 잡는다. 숫자가 바뀌어도 글자가 위아래로 흔들리지 않는다.
	const FGlyphInfo* Cap = Font.GlyphMap.Find('H');
	const float CapHeight = (Cap ? Cap->PlaneTop : 0.7f) * FontSize;
	const float Baseline = Rect.Y + (Rect.H + CapHeight) * 0.5f;

	uint32 Prev = 0;
	for (uint32 CP : DecodeUTF8(Text))
	{
		const FGlyphInfo* Glyph = Font.GlyphMap.Find(CP);
		if (!Glyph) continue;
		if (Prev)
			if (const float* Kern = Font.KerningMap.Find((static_cast<uint64>(Prev) << 32) | CP))
				PenX += *Kern * FontSize;

		if (Glyph->bHasBounds)
		{
			// 폰트 좌표는 Y가 위로, 화면 좌표는 아래로 증가한다.
			const float L = PenX + Glyph->PlaneLeft * FontSize;
			const float R = PenX + Glyph->PlaneRight * FontSize;
			const float T = Baseline - Glyph->PlaneTop * FontSize;
			const float B = Baseline - Glyph->PlaneBottom * FontSize;

			// 아틀라스는 아래가 원점이다 (FTextRenderer::BuildTextMesh와 같은 변환).
			const float U0 = Glyph->AtlasLeft / Font.Width;
			const float U1 = Glyph->AtlasRight / Font.Width;
			const float V0 = 1.0f - Glyph->AtlasTop / Font.Height;
			const float V1 = 1.0f - Glyph->AtlasBottom / Font.Height;

			AddQuad(L, T, R, B, U0, V0, U1, V1, Color, EUIDrawMode::Text, Font.AtlasTexture, Font.DistanceRange);
		}
		PenX += Glyph->Advance * FontSize;
		Prev = CP;
	}
}