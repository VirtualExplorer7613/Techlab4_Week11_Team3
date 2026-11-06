#include "EnginePCH.h"
#include "Texture2D.h"

#include "RenderCommand.h"


FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image)
	: FTexture2D(Device, InDesc, Image.IsValid() ? Image.Pixels.GetData() : nullptr, Image.GetRowPitch())
{
}

FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch)
{
	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	Dimension = ETextureDimension::Texture2D;

	if (InitialData && RowPitch == 0)
	{
		const uint32 BytesPerPixel = FormatToBytes(InDesc.Format);
		if (BytesPerPixel == 0) { /* 기존 에러 처리 그대로 */ return; }
		RowPitch = InDesc.Width * BytesPerPixel;
	}

	const bool bGenerateMips = (InDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS) != 0;

	D3D11_SUBRESOURCE_DATA SubData{ InitialData, RowPitch, 0 };
	const D3D11_SUBRESOURCE_DATA* InitPtr = (InitialData && !bGenerateMips) ? &SubData : nullptr;

	HRESULT hr = Device->CreateTexture2D(&InDesc, InitPtr, (ID3D11Texture2D**)Texture.GetAddressOf());
	if (FAILED(hr)) { /* 기존 로그 그대로 */ return; }

	CreateViews(Device, InDesc);

	if (bGenerateMips && InitialData && SRV)
	{
		ComPtr<ID3D11DeviceContext> Context;
		Device->GetImmediateContext(Context.GetAddressOf());
		Context->UpdateSubresource(Texture.Get(), 0, nullptr, InitialData, RowPitch, 0); // 0번 레벨
		Context->GenerateMips(SRV.Get());                                              // 나머지 레벨
	}
}

FTexture2D::FTexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc)
{
	Texture = std::move(SwapchainTexture);

	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	Dimension = ETextureDimension::Texture2D;

	CreateViews(Device, InDesc);
}

void FTexture2D::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc)
{
	HRESULT hr;

	// TYPELESS 깊이 텍스처는 뷰마다 해석할 포맷을 지정해야 한다 (nullptr이면 생성 실패)
	const bool bTypelessDepth = (InDesc.Format == DXGI_FORMAT_R24G8_TYPELESS);

	if (InDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
		SrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;   // 깊이 24bit만 float로 읽는다
		SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SrvDesc.Texture2D.MipLevels = 1;

		hr = Device->CreateShaderResourceView(Texture.Get(), bTypelessDepth ? &SrvDesc : nullptr, SRV.GetAddressOf());
		if (FAILED(hr))
			HTR_LOG(Error, "[Texture2D] CreateShaderResourceView failed (hr=0x{:08X})", (uint32)hr);
	}

	if ((InDesc.BindFlags & D3D11_BIND_RENDER_TARGET) &&
		!(InDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS))
	{
		hr = Device->CreateRenderTargetView(Texture.Get(), nullptr, RTV.GetAddressOf());
		if (FAILED(hr))
			HTR_LOG(Error, "[Texture2D] CreateRenderTargetView failed (hr=0x{:08X})", (uint32)hr);
	}

	if (InDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{};
		DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

		hr = Device->CreateDepthStencilView(Texture.Get(), bTypelessDepth ? &DsvDesc : nullptr, DSV.GetAddressOf());
		if (FAILED(hr))
			HTR_LOG(Error, "[Texture2D] CreateDepthStencilView failed (hr=0x{:08X})", (uint32)hr);
	}
}

UTexture2D::UTexture2D()
{

}
UTexture2D::~UTexture2D()
{
}

FTexture2DArray::FTexture2DArray(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch)
{
	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	Dimension = ETextureDimension::Texture2DArray;
	ArraySize = InDesc.ArraySize;

	if (InitialData && RowPitch == 0)
	{
		const uint32 BytesPerPixel = FormatToBytes(InDesc.Format);
		if (BytesPerPixel == 0) { /* 기존 에러 처리 그대로 */ return; }
		RowPitch = InDesc.Width * BytesPerPixel;
	}

	const bool bGenerateMips = (InDesc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS) != 0;

	D3D11_SUBRESOURCE_DATA SubData{ InitialData, RowPitch, 0 };
	const D3D11_SUBRESOURCE_DATA* InitPtr = (InitialData && !bGenerateMips) ? &SubData : nullptr;

	HRESULT hr = Device->CreateTexture2D(&InDesc, InitPtr, (ID3D11Texture2D**)Texture.GetAddressOf());
	if (FAILED(hr)) { /* 기존 로그 그대로 */ return; }

	CreateViews(Device, InDesc);

	if (bGenerateMips && InitialData && SRV)
	{
		ComPtr<ID3D11DeviceContext> Context;
		Device->GetImmediateContext(Context.GetAddressOf());
		Context->UpdateSubresource(Texture.Get(), 0, nullptr, InitialData, RowPitch, 0); // 0번 레벨
		Context->GenerateMips(SRV.Get());                                              // 나머지 레벨
	}
}

FTexture2DArray::FTexture2DArray(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image)
	: FTexture2DArray(Device, InDesc, Image.IsValid() ? Image.Pixels.GetData() : nullptr, Image.GetRowPitch())
{
}

void FTexture2DArray::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc)
{
	SRV.Reset();
	RTV.Reset();
	DSV.Reset();
	RTVs.Reset();
	DSVs.Reset();

	if (!Texture || InDesc.ArraySize == 0)
	{
		return;
	}

	const bool bMSAA = InDesc.SampleDesc.Count > 1;

	DXGI_FORMAT SrvFormat = InDesc.Format;
	DXGI_FORMAT DsvFormat = InDesc.Format;
	switch (InDesc.Format)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:
		SrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		DsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;
	case DXGI_FORMAT_R32_TYPELESS:
		SrvFormat = DXGI_FORMAT_R32_FLOAT;
		DsvFormat = DXGI_FORMAT_D32_FLOAT;
		break;
	case DXGI_FORMAT_R16_TYPELESS:
		SrvFormat = DXGI_FORMAT_R16_UNORM;
		DsvFormat = DXGI_FORMAT_D16_UNORM;
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		SrvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		DsvFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		break;
	}

	if (InDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC Desc{};
		Desc.Format = SrvFormat;
		Desc.ViewDimension = bMSAA ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		if (bMSAA)
		{
			Desc.Texture2DMSArray.ArraySize = InDesc.ArraySize;
		}
		else
		{
			Desc.Texture2DArray.MipLevels = InDesc.MipLevels ? InDesc.MipLevels : UINT(-1);
			Desc.Texture2DArray.ArraySize = InDesc.ArraySize;
		}

		const HRESULT Result = Device->CreateShaderResourceView(Texture.Get(), &Desc, SRV.GetAddressOf());
		if (FAILED(Result))
		{
			HTR_LOG(Error, "[Texture2DArray] CreateShaderResourceView failed (hr=0x{:08X})", (uint32)Result);
		}
	}

	if (InDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		D3D11_RENDER_TARGET_VIEW_DESC Desc{};
		Desc.Format = InDesc.Format;
		Desc.ViewDimension = bMSAA ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		if (bMSAA)
		{
			Desc.Texture2DMSArray.ArraySize = InDesc.ArraySize;
		}
		else
		{
			Desc.Texture2DArray.ArraySize = InDesc.ArraySize;
		}

		HRESULT Result = Device->CreateRenderTargetView(Texture.Get(), &Desc, RTV.GetAddressOf());
		if (FAILED(Result))
		{
			HTR_LOG(Error, "[Texture2DArray] CreateRenderTargetView failed (hr=0x{:08X})", (uint32)Result);
		}

		RTVs.SetNum(InDesc.ArraySize);
		for (uint32 Slice = 0; Slice < InDesc.ArraySize; ++Slice)
		{
			if (bMSAA)
			{
				Desc.Texture2DMSArray.FirstArraySlice = Slice;
				Desc.Texture2DMSArray.ArraySize = 1;
			}
			else
			{
				Desc.Texture2DArray.FirstArraySlice = Slice;
				Desc.Texture2DArray.ArraySize = 1;
			}
			Result = Device->CreateRenderTargetView(Texture.Get(), &Desc, RTVs[Slice].GetAddressOf());
			if (FAILED(Result))
			{
				HTR_LOG(Error, "[Texture2DArray] CreateRenderTargetView slice={} failed (hr=0x{:08X})", Slice, (uint32)Result);
			}
		}
	}

	if (InDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC Desc{};
		Desc.Format = DsvFormat;
		Desc.ViewDimension = bMSAA ? D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY : D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		if (bMSAA)
		{
			Desc.Texture2DMSArray.ArraySize = InDesc.ArraySize;
		}
		else
		{
			Desc.Texture2DArray.ArraySize = InDesc.ArraySize;
		}
		HRESULT Result = Device->CreateDepthStencilView(Texture.Get(), &Desc, DSV.GetAddressOf());
		if (FAILED(Result))
		{
			HTR_LOG(Error, "[Texture2DArray] CreateDepthStencilView failed (hr=0x{:08X})", (uint32)Result);
		}

		DSVs.SetNum(InDesc.ArraySize);
		for (uint32 Slice = 0; Slice < InDesc.ArraySize; ++Slice)
		{
			if (bMSAA)
			{
				Desc.Texture2DMSArray.FirstArraySlice = Slice;
				Desc.Texture2DMSArray.ArraySize = 1;
			}
			else
			{
				Desc.Texture2DArray.FirstArraySlice = Slice;
				Desc.Texture2DArray.ArraySize = 1;
			}
			Result = Device->CreateDepthStencilView(Texture.Get(), &Desc, DSVs[Slice].GetAddressOf());
			if (FAILED(Result))
			{
				HTR_LOG(Error, "[Texture2DArray] CreateDepthStencilView slice={} failed (hr=0x{:08X})", Slice, (uint32)Result);
			}
		}
	}
}
