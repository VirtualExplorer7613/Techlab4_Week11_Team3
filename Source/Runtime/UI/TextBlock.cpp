#include "EnginePCH.h"
#include "UI/TextBlock.h"

#include "Asset/AssetManager.h"
#include "Text/Font.h"

UTextBlock::UTextBlock()
{
	bAutoSize = true;    // 글자 길이에 맞춰 크기가 정해진다
	ResolveFont();
}

void UTextBlock::ResolveFont()
{
	Font = UAssetManager::GetAssetByPath<UFont>(FontPath);
}

void UTextBlock::PostEditChangeProperty(const FProperty& Property)
{
	if (Property.Name == "FontPath")
		ResolveFont();
}

FVector2 UTextBlock::GetDesiredSize() const
{
	return Font ? FUIDrawList::MeasureText(Text, *Font, FontSize) : Super::GetDesiredSize();
}

void UTextBlock::Paint(FUIDrawList& DrawList, float ParentOpacity) const
{
	if (!IsVisible() || !Font) return;
	FLinearColor Color = ColorAndOpacity;
	Color.A *= ParentOpacity * RenderOpacity;
	DrawList.AddText(CachedRect, Text, *Font, FontSize * CachedScale, Color, Justification);
}