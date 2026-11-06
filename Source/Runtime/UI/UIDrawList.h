#pragma once

#include "Math/Vector2.h"
#include "Math/Color.h"

class UTexture2D;
class UFont;

// 화면 위 사각형 하나 (픽셀, 좌상단 원점)
struct FUIRect
{
	float X = 0.0f, Y = 0.0f, W = 0.0f, H = 0.0f;

	bool Contains(float PX, float PY) const { return PX >= X && PX < X + W && PY >= Y && PY < Y + H; }
};

enum class EUIDrawMode : uint32 { Solid = 0, Texture = 1, Text = 2 };

// UIShader.hlsl의 VSInput과 순서·크기가 같아야 한다.
struct FUIVertex
{
	FVector2 Position;   // 픽셀 좌표
	FVector2 UV;
	FLinearColor Color;
	float Mode;          // EUIDrawMode
};

// 같은 텍스처를 쓰는 연속 인덱스 구간
struct FUIDrawBatch
{
	UTexture2D* Texture = nullptr;
	float PxRange = 0.0f;        // MSDF distance range (글자 배치만 사용)
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
};

class FUIDrawList
{
public:
	void Reset();

	void AddRect(const FUIRect& Rect, const FLinearColor& Color);
	void AddImage(const FUIRect& Rect, UTexture2D* Texture, const FLinearColor& Tint);
	// Justify: 0 왼쪽, 1 가운데, 2 오른쪽. 세로는 항상 가운데.
	void AddText(const FUIRect& Rect, const FString& Text, const UFont& Font, float FontSize,
		const FLinearColor& Color, int32 Justify);

	// FontSize 기준 글자 폭과 한 줄 높이
	static FVector2 MeasureText(const FString& Text, const UFont& Font, float FontSize);

	const TArray<FUIVertex>& GetVertices() const { return Vertices; }
	const TArray<uint32>& GetIndices() const { return Indices; }
	const TArray<FUIDrawBatch>& GetBatches() const { return Batches; }

private:
	void AddQuad(float L, float T, float R, float B, float U0, float V0, float U1, float V1,
		const FLinearColor& Color, EUIDrawMode Mode, UTexture2D* Texture, float PxRange);

	TArray<FUIVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FUIDrawBatch> Batches;
};