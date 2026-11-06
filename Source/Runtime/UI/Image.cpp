
#include "EnginePCH.h"
#include "UI/Image.h"

#include "Asset/AssetManager.h"

void UImage::ResolveTexture()
{
	// UI 텍스처는 밉맵이 필요 없다.
	Texture = TexturePath.empty() ? nullptr : UAssetManager::Get().LoadTexture(TexturePath, false);
}

void UImage::PostEditChangeProperty(const FProperty& Property)
{
	if (Property.Name == "TexturePath")
		ResolveTexture();
}

void UImage::Paint(FUIDrawList& DrawList, float ParentOpacity) const
{
	if (!IsVisible()) return;
	FLinearColor Color = ColorAndOpacity;
	Color.A *= ParentOpacity * RenderOpacity;
	if (Texture) DrawList.AddImage(CachedRect, Texture, Color);
	else DrawList.AddRect(CachedRect, Color);
}