#include "EnginePCH.h"
#include "UI/Widget.h"
#include "UI/PanelWidget.h"
#include "UI/UserWidget.h"

namespace
{
	// 한 축(가로 또는 세로)의 위치와 크기를 구한다.
	void SolveAxis(float ParentMin, float ParentSize, float AnchorMin, float AnchorMax,
		float OffsetStart, float OffsetEnd, float DesiredSize, float Alignment, float Scale,
		float& OutPos, float& OutSize)
	{
		const float A0 = ParentMin + ParentSize * AnchorMin;
		const float A1 = ParentMin + ParentSize * AnchorMax;

		if (AnchorMin == AnchorMax)
		{
			// 앵커가 한 점: OffsetStart = 위치, DesiredSize = 크기, Alignment로 기준점 이동
			OutSize = DesiredSize * Scale;
			OutPos = A0 + OffsetStart * Scale - OutSize * Alignment;
		}
		else
		{
			// 앵커가 벌어짐: 양쪽 여백
			OutPos = A0 + OffsetStart * Scale;
			OutSize = (A1 - OffsetEnd * Scale) - OutPos;
		}
	}
}

UWidget::~UWidget()
{
	if (Parent)
		Parent->RemoveChild(this);
	if (RootOwner)
		RootOwner->RootWidget = nullptr;
}

void UWidget::Arrange(const FUIGeometry& ParentGeometry)
{
	const FUIRect& P = ParentGeometry.Rect;
	const float S = ParentGeometry.Scale;
	const FVector2 Desired = bAutoSize ? GetDesiredSize() : FVector2(Offsets.Z, Offsets.W);

	FUIRect R;
	SolveAxis(P.X, P.W, Anchors.X, Anchors.Z, Offsets.X, Offsets.Z, Desired.X, AlignmentX, S, R.X, R.W);
	SolveAxis(P.Y, P.H, Anchors.Y, Anchors.W, Offsets.Y, Offsets.W, Desired.Y, AlignmentY, S, R.Y, R.H);
	SetGeometry(R, S);
}

void UWidget::SetGeometry(const FUIRect& InRect, float InScale)
{
	CachedRect = InRect;
	CachedScale = InScale;
}

UWidget* UWidget::HitTest(float X, float Y)
{
	return (IsHitTestable() && CachedRect.Contains(X, Y)) ? this : nullptr;
}

void UWidget::RemoveFromParent()
{
	if (Parent)
		Parent->RemoveChild(this);
}