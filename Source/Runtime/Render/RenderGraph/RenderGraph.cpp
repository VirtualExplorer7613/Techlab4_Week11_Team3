#include "EnginePCH.h"
#include "Render/RenderGraph/RenderGraph.h"

#include "Render/RenderCommand.h"
#include "Render/Texture2D.h"

#include <algorithm>
#include <utility>

namespace
{
	// D3D11은 같은 리소스를 SRV와 RTV/DSV/UAV로 동시에 사용할 수 없다.
	// 현재 상태를 직접 조회하므로 RenderCommand 밖에서 바인딩한 SRV도 처리한다.
	template <typename TGet, typename TSet>
	void UnbindConflictingSRVs(TGet GetViews, TSet SetViews,
		const std::vector<ID3D11Resource*>& Outputs)
	{
		ID3D11ShaderResourceView* Views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
		GetViews(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, Views);
		for (uint32 Slot = 0; Slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++Slot)
		{
		ID3D11ShaderResourceView* View = Views[Slot];
		if (!View) continue;
		ID3D11Resource* Resource = nullptr;
		View->GetResource(&Resource);
		const bool bConflict = std::find(Outputs.begin(), Outputs.end(), Resource) != Outputs.end();
		if (bConflict)
		{
			ID3D11ShaderResourceView* NullView = nullptr;
			SetViews(Slot, 1, &NullView);
		}
		if (Resource) Resource->Release();
		View->Release(); // GetShaderResources가 반환한 참조를 해제한다.
		}
	}

	void UnbindConflictingViews(ID3D11DeviceContext* Context,
		const std::vector<ID3D11Resource*>& Outputs)
	{
		// 이번 패스가 RTV/DSV로 쓸 리소스가 이전 셰이더의 SRV 슬롯에 남아 있으면 해제한다.
		// 동일한 검사 함수를 VS, PS, GS, HS, DS, CS의 조회/설정 함수에 각각 적용한다.
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->VSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->VSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->PSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->PSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->GSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->GSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->HSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->HSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->DSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->DSSetShaderResources(Start, Count, Views); }, Outputs);
		UnbindConflictingSRVs(
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView** Views) { Context->CSGetShaderResources(Start, Count, Views); },
			[Context](UINT Start, UINT Count, ID3D11ShaderResourceView* const* Views) { Context->CSSetShaderResources(Start, Count, Views); }, Outputs);

		// 컴퓨트 셰이더가 쓰기용 UAV로 묶어 둔 리소스도 이번 출력과 겹치면 해제한다.
		ID3D11UnorderedAccessView* UAVs[D3D11_PS_CS_UAV_REGISTER_COUNT]{};
		Context->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, UAVs);
		for (uint32 Slot = 0; Slot < D3D11_PS_CS_UAV_REGISTER_COUNT; ++Slot)
		{
			ID3D11UnorderedAccessView* View = UAVs[Slot];
			if (!View) continue;
			ID3D11Resource* Resource = nullptr;
			// UAV와 RTV/DSV가 서로 다른 뷰여도 실제 텍스처가 같으면 충돌한다.
			View->GetResource(&Resource);
			if (std::find(Outputs.begin(), Outputs.end(), Resource) != Outputs.end())
			{
				ID3D11UnorderedAccessView* NullView = nullptr;
				Context->CSSetUnorderedAccessViews(Slot, 1, &NullView, nullptr);
			}
			// GetResource와 CSGetUnorderedAccessViews가 조회하면서 늘린 참조만 반환한다.
			if (Resource) Resource->Release();
			View->Release();
		}
	}
	struct FPooledTexture
	{
		TUniquePtr<FTexture2D> Texture;
		FRenderGraphTextureDesc Desc{};
		bool bInUse = false;
	};
	static std::vector<FPooledTexture> GTexturePool;

	FTexture2D* AcquireTexture(const FRenderGraphTextureDesc& Desc)
	{
		for (auto& Item : GTexturePool)
		{
			if (!Item.bInUse && Item.Texture &&
				Item.Desc.Width == Desc.Width &&
				Item.Desc.Height == Desc.Height &&
				Item.Desc.Format == Desc.Format &&
				Item.Desc.BindFlags == Desc.BindFlags)
			{
				Item.bInUse = true;
				return Item.Texture.get();
			}
		}

		FTextureDesc TextureDesc{};
		TextureDesc.Width = Desc.Width;
		TextureDesc.Height = Desc.Height;
		TextureDesc.BindFlags = Desc.BindFlags;
		TextureDesc.Format = Desc.Format;

		auto NewTex = RenderCommand::CreateTexture2D(TextureDesc);
		if (!NewTex || !NewTex->GetRawPtr() ||
			(HasFlag(Desc.BindFlags, ETextureBindFlags::RenderTarget) && !NewTex->GetRTV()) ||
			(HasFlag(Desc.BindFlags, ETextureBindFlags::DepthStencil) && !NewTex->GetDSV()) ||
			(HasFlag(Desc.BindFlags, ETextureBindFlags::ShaderResource) && !NewTex->GetSRV())) return nullptr;
		FTexture2D* Ptr = NewTex.get();
		GTexturePool.push_back({ std::move(NewTex), Desc, true });
		return Ptr;
	}
}

FRenderGraphBuilder::~FRenderGraphBuilder()
{
	for (auto& Entry : Textures)
	{
		if (Entry.bTransient && Entry.Texture)
		{
			for (auto& Item : GTexturePool)
			{
				if (Item.Texture.get() == Entry.Texture)
				{
					Item.bInUse = false;
					break;
				}
			}
		}
	}
}

FRenderGraphTextureRef FRenderGraphBuilder::ImportTexture(FTexture2D* Texture, const char* Name)
{
	if (bExecuted || !Texture || !Texture->GetRawPtr() || !Texture->GetWidth() || !Texture->GetHeight())
		return {};
	for (uint32 Index = 0; Index < Textures.size(); ++Index)
	{
		if (Textures[Index].Texture == Texture)
			return { Index, this };
	}
	const uint32 Index = static_cast<uint32>(Textures.size());
	FImportedTexture Entry{};
	Entry.Texture = Texture;
	Entry.Name = Name ? Name : "";
	Entry.Desc.Width = Texture->GetWidth();
	Entry.Desc.Height = Texture->GetHeight();
	Entry.Desc.Format = Texture->GetTextureFormat();
	Textures.push_back(std::move(Entry));
	return { Index, this };
}

FRenderGraphTextureRef FRenderGraphBuilder::CreateTexture(const FRenderGraphTextureDesc& Desc, const char* Name)
{
	if (bExecuted || Desc.Width == 0 || Desc.Height == 0)
		return {};
	const uint32 Index = static_cast<uint32>(Textures.size());
	FImportedTexture Entry{};
	Entry.Name = Name ? Name : "";
	Entry.Desc = Desc;
	Entry.bTransient = true;
	Textures.push_back(std::move(Entry));
	return { Index, this };
}

FTexture2D* FRenderGraphBuilder::Resolve(FRenderGraphTextureRef Ref) const
{
	return Ref.Owner == this && Ref.Index < Textures.size() ? Textures[Ref.Index].Texture : nullptr;
}

bool FRenderGraphBuilder::ValidatePass(const FRenderGraphRasterPassDesc& Desc) const
{
	return !(Desc.ColorAttachments.empty() && !Desc.DepthAttachment.Texture);
}

bool FRenderGraphBuilder::AddRasterPass(FRenderGraphRasterPassDesc Desc, std::function<void()> Execute)
{
	if (bExecuted || !Execute) return false;
	uint32 Width = 0, Height = 0;
	if (!ValidatePass(Desc)) return false;
	FRasterPass Pass{};
	Pass.Desc = std::move(Desc);
	Pass.Execute = std::move(Execute);
	Passes.push_back(std::move(Pass));
	return true;
}

bool FRenderGraphBuilder::AddComputePass(FRenderGraphComputePassDesc Desc, std::function<void()> Execute)
{
	if (bExecuted || !Execute || Desc.Name.empty() || Desc.WriteResources.empty()) return false;
	for (ID3D11Resource* Resource : Desc.ReadResources)
		if (!Resource) return false;
	for (ID3D11Resource* Resource : Desc.WriteResources)
	{
		if (!Resource || std::find(Desc.ReadResources.begin(), Desc.ReadResources.end(), Resource)
			!= Desc.ReadResources.end()) return false;
		if (std::count(Desc.WriteResources.begin(), Desc.WriteResources.end(), Resource) != 1) return false;
	}
	FRasterPass Pass{};
	Pass.ComputeDesc = std::move(Desc);
	Pass.Execute = std::move(Execute);
	Pass.bCompute = true;
	Passes.push_back(std::move(Pass));
	return true;
}

bool FRenderGraphBuilder::Execute() // 등록되어 있던 
{
	if (bExecuted || !FRenderDevice::Get()) return false;
	bExecuted = true;

	for (auto& Entry : Textures)
	{
		if (Entry.bTransient && !Entry.Texture)
		{
			Entry.Texture = AcquireTexture(Entry.Desc);
			if (!Entry.Texture) return false;
		}
	}

	// 바인딩 충돌 해제는 현재 D3D11 상태를 직접 조회해야 하므로 컨텍스트를 그대로 쓴다.
	ID3D11DeviceContext* Context = FRenderDevice::Get()->GetContext();
	for (FRasterPass& Pass : Passes)
	{
		if (Pass.bCompute)
		{
			RenderCommand::EndRenderPass();
			UnbindConflictingViews(Context, Pass.ComputeDesc.ReadResources); // 이전 패스에서 같은 리소스가 UAV(쓰기)로 묶여 있으면 해제
			UnbindConflictingViews(Context, Pass.ComputeDesc.WriteResources); // 같은 리소스가 이전 패스의 SRV(읽기) 또는 UAV(쓰기)로 묶여 있으면 해제
			Pass.Execute(); 
			ID3D11UnorderedAccessView* NullUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT]{};
			Context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, NullUAVs, nullptr);
			continue;
		}

		// 이전 패스의 출력 바인딩을 먼저 해제해야 입력 SRV로 사용할 수 있다.
		RenderCommand::EndRenderPass();

		std::vector<ID3D11Resource*> Outputs;
		FTexture2D* ColorTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};

		uint32 TexWidth = 0;
		uint32 TexHeight = 0;

		for (uint32 Index = 0; Index < Pass.Desc.ColorAttachments.size(); ++Index)
		{
			FTexture2D* Texture = Resolve(Pass.Desc.ColorAttachments[Index].Texture);
			ColorTargets[Index] = Texture;
			if (Texture)
			{
				Outputs.push_back(Texture->GetRawPtr());
				TexWidth = Texture->GetWidth();
				TexHeight = Texture->GetHeight();
			}
		}

		FTexture2D* Depth = Resolve(Pass.Desc.DepthAttachment.Texture);
		if (Depth)
		{
			Outputs.push_back(Depth->GetRawPtr());
			TexWidth = Depth->GetWidth();
			TexHeight = Depth->GetHeight();
		}

		UnbindConflictingViews(Context, Outputs);
		
		for (uint32 Index = 0; Index < Pass.Desc.ColorAttachments.size(); ++Index)
		{
			const FRenderGraphColorAttachment& Attachment = Pass.Desc.ColorAttachments[Index];
			if (Attachment.LoadAction == ERenderGraphLoadAction::Clear && ColorTargets[Index] && ColorTargets[Index]->GetRTV())
			{
				const float Color[4] = { Attachment.ClearColor[0], Attachment.ClearColor[1],
					Attachment.ClearColor[2], Attachment.ClearColor[3] };
				RenderCommand::ClearRenderTarget(ColorTargets[Index], Color);
			}
		}

		if (Pass.Desc.DepthAttachment.LoadAction == ERenderGraphLoadAction::Clear)
		{
			RenderCommand::ClearDepthStencil(Resolve(Pass.Desc.DepthAttachment.Texture), Pass.Desc.DepthAttachment.ClearDepth, Pass.Desc.DepthAttachment.ClearStencil);
		}

		RenderCommand::SetRenderTargets(static_cast<uint32>(Pass.Desc.ColorAttachments.size()), ColorTargets,
			Depth, Pass.Desc.DepthAttachment.DSVIndex);
		for (uint32 Slot = 0; Slot < Pass.Desc.ReadTextures.size(); ++Slot)
			RenderCommand::BindShaderResource(Slot, Resolve(Pass.Desc.ReadTextures[Slot]), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

		FShadowAtlasRect Viewport = Pass.Desc.Viewport;
		if (Viewport.Width == 0)
		{
			Viewport.Width = TexWidth;
		}
		if (Viewport.Height == 0)
		{
			Viewport.Height = TexHeight;
		}

		RenderCommand::SetViewport(Viewport.X, Viewport.Y, Viewport.Width, Viewport.Height);
		Pass.Execute();
		RenderCommand::EndRenderPass();
	}
	return true;
}

