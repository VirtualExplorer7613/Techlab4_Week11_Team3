
#include "EnginePCH.h"
#include "UI/Button.h"

void UButton::Paint(FUIDrawList& DrawList, float ParentOpacity) const
{
	if (!IsVisible()) return;
	FLinearColor Color = bPressed ? PressedColor : (bHovered ? HoveredColor : NormalColor);
	Color.A *= ParentOpacity * RenderOpacity;
	DrawList.AddRect(CachedRect, Color);
	Super::Paint(DrawList, ParentOpacity);   // 라벨
}