#pragma once

#include <d3d11.h>
#include "Asset/RenderAsset.h"
#include "Render/RenderDevice.h"
#include "Texture.h"
#include "ImageLoader.h"
#include "Texture2D.generated.h"

class FTexture2D : public FTexture
{
public:
	// RowPitch가 0이면 InDesc.Format에서 계산한다.
	FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch = 0);
	// 로더가 포맷과 픽치까지 알고 있으므로 그대로 받는다.
	FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image);
	// For Swapchain
	FTexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc);
	virtual ~FTexture2D() = default;

protected:
	FTexture2D() = default;

private:
	void CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc);
};

UCLASS()
class UTexture2D : public URenderAsset
{
	GENERATED_BODY()
	
	friend class UAssetManager;
public:
	UTexture2D();
	~UTexture2D();

	uint32 GetWidth()  const { return Texture ? Texture->GetWidth() : 0; }
	uint32 GetHeight() const { return Texture ? Texture->GetHeight() : 0; }

	FTexture2D* GetResource() const { return Texture.get(); }
private:
	void SetResource(TUniquePtr<FTexture2D> InResource) { Texture = std::move(InResource); }

	TUniquePtr<FTexture2D> Texture;
};

class FTexture2DArray : public FTexture2D
{
public:
	FTexture2DArray(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch = 0);
	FTexture2DArray(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image);

	~FTexture2DArray() = default;

	using FTexture::GetRTV;
	using FTexture::GetDSV;

	ID3D11RenderTargetView* GetRTV(uint32 Slice) const { return Slice < static_cast<uint32>(RTVs.Num()) ? RTVs[Slice].Get() : nullptr; }
	ID3D11DepthStencilView* GetDSV(uint32 Slice) const { return Slice < static_cast<uint32>(DSVs.Num()) ? DSVs[Slice].Get() : nullptr; }

	inline uint32 GetArraySize() { return ArraySize; }

private:
	void CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc);

	uint32 ArraySize = 0;

	TArray<ComPtr<ID3D11RenderTargetView>> RTVs;
	TArray<ComPtr<ID3D11DepthStencilView>> DSVs;
};
