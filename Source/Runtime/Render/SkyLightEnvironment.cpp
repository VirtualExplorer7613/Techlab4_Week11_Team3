#include "EnginePCH.h"
#include "Render/SkyLightEnvironment.h"
#include "Render/ImageLoader.h"
#include "Render/RenderCommand.h"
#include "Render/RenderUtil.h"
#include "Render/Texture2D.h"
#include "Render/Renderer.h"

namespace
{
	constexpr uint32 EnvironmentSize = 128, IrradianceSize = 32, IntegrationSamples = 512;
	struct FSkyConstants
	{
		uint32 OutputSize, PanoramaHeight, bSourceIsLinear, SampleCount;
	};
	static_assert(sizeof(FSkyConstants) == 16);

	TUniquePtr<FComputeShader> CompileCompute(const char* Entry)
	{
		TUniquePtr<FComputeShader> Shader = RenderCommand::CreateComputeShader(RenderUtil::CompileShader(
			"Resources/Shader/SkyLightConvolutionCS.hlsli", Entry, EShaderType::Compute));
		if (!Shader || !Shader->IsValid())
		{
			HTR_LOG(Error, "[SkyLight] {} compile failed", Entry);
			return nullptr;
		}
		return Shader;
	}

	TUniquePtr<FTextureCube> CreateCube(uint32 Size)
	{
		FTextureDesc Desc{};
		Desc.Width = Desc.Height = Size;
		Desc.Format = ETextureFormat::R32G32B32A32_Float;
		Desc.BindFlags = ETextureBindFlags::ShaderResource | ETextureBindFlags::UnorderedAccess;
		auto Cube = RenderCommand::CreateTextureCube(Desc, {});
		return Cube && Cube->GetSRV() && Cube->GetUAV() ? std::move(Cube) : nullptr;
	}
}

bool FSkyLightEnvironment::Prepare(const FString& InSourcePath)
{
	FImageData Image = ImageLoader::LoadAuto(InSourcePath);
	if (!Image.IsValid() || (Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
		Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
		Image.Format != DXGI_FORMAT_R32G32B32A32_FLOAT))
	{
		HTR_LOG(Warning, "[SkyLight] unsupported panorama: {}", InSourcePath);
		return false;
	}

	FTextureDesc PanoramaDesc{};
	PanoramaDesc.Width = Image.Width;
	PanoramaDesc.Height = Image.Height;
	PanoramaDesc.MipLevels = Image.MipLevels;
	PanoramaDesc.Usage = ETextureUsage::Immutable;
	PanoramaDesc.BindFlags = ETextureBindFlags::ShaderResource;
	auto NewPanorama = RenderCommand::CreateTexture2D(PanoramaDesc, Image);
	auto NewEnvironment = CreateCube(EnvironmentSize);
	auto NewIrradiance = CreateCube(IrradianceSize);
	if (!NewPanorama || !NewPanorama->GetSRV() || !NewEnvironment || !NewIrradiance)
	{
		HTR_LOG(Warning, "[SkyLight] GPU texture creation failed: {}", InSourcePath);
		return false;
	}

	TUniquePtr<FComputeShader> NewEnvironmentCS = CompileCompute("BuildEnvironmentCube");
	TUniquePtr<FComputeShader> NewIrradianceCS = CompileCompute("BuildIrradianceCube");
	if (!NewEnvironmentCS || !NewIrradianceCS) return false;

	TUniquePtr<FConstantBuffer> NewConstantsCB = RenderCommand::CreateConstantBuffer(sizeof(FSkyConstants));
	if (!NewConstantsCB) return false;

	Panorama = std::move(NewPanorama);
	EnvironmentCube = std::move(NewEnvironment);
	IrradianceCube = std::move(NewIrradiance);
	EnvironmentCS = std::move(NewEnvironmentCS);
	IrradianceCS = std::move(NewIrradianceCS);
	ConstantsCB = std::move(NewConstantsCB);
	PanoramaHeight = Image.Height;
	bSourceIsLinear = Image.Format != DXGI_FORMAT_R8G8B8A8_UNORM;
	SourcePath = InSourcePath;
	return true;
}

void FSkyLightEnvironment::DispatchEnvironment(FRenderer* Renderer)
{
	const FSkyConstants Constants{ EnvironmentSize, PanoramaHeight, bSourceIsLinear ? 1u : 0u,
		IntegrationSamples };
	RenderCommand::UpdateBufferData(ConstantsCB.get(), &Constants, sizeof(Constants));
	RenderCommand::BindComputeShader(EnvironmentCS.get());
	RenderCommand::BindConstantBuffer(0, ConstantsCB.get(), EShaderBindFlagBits::Compute);
	RenderCommand::BindShaderResource(0, Panorama.get(), EShaderBindFlagBits::Compute);
	RenderCommand::BindUnorderedAccess(0, EnvironmentCube.get());
	RenderCommand::BindSamplerState(0, ESamplerState::LinearWrapUClampV, EShaderBindFlagBits::Compute);
	RenderCommand::Dispatch(EnvironmentSize / 8, EnvironmentSize / 8, FTextureCube::FaceCount);

	RenderCommand::UnbindShaderResources(0, 1, EShaderBindFlagBits::Compute);
	RenderCommand::BindConstantBuffer(0, nullptr, EShaderBindFlagBits::Compute);
	RenderCommand::BindComputeShader(nullptr);
}

void FSkyLightEnvironment::DispatchIrradiance(FRenderer* Renderer)
{
	const FSkyConstants Constants{ IrradianceSize, PanoramaHeight, 1u, IntegrationSamples };
	RenderCommand::UpdateBufferData(ConstantsCB.get(), &Constants, sizeof(Constants));
	RenderCommand::BindComputeShader(IrradianceCS.get());
	RenderCommand::BindConstantBuffer(0, ConstantsCB.get(), EShaderBindFlagBits::Compute);
	RenderCommand::BindShaderResource(1, EnvironmentCube.get(), EShaderBindFlagBits::Compute);
	RenderCommand::BindUnorderedAccess(0, IrradianceCube.get());
	RenderCommand::BindSamplerState(0, ESamplerState::LinearWrapUClampV, EShaderBindFlagBits::Compute);
	RenderCommand::Dispatch(IrradianceSize / 8, IrradianceSize / 8, FTextureCube::FaceCount);

	RenderCommand::UnbindShaderResources(1, 1, EShaderBindFlagBits::Compute);
	RenderCommand::BindConstantBuffer(0, nullptr, EShaderBindFlagBits::Compute);
	RenderCommand::BindComputeShader(nullptr);
}

void FSkyLightEnvironment::Complete()
{
	bReady = true;
	Panorama.reset();
	EnvironmentCS.reset();
	IrradianceCS.reset();
	ConstantsCB.reset();
	HTR_LOG(Info, "[SkyLight] GPU environment {}x{}, irradiance {}x{}: {}",
		EnvironmentSize, EnvironmentSize, IrradianceSize, IrradianceSize, SourcePath);
}

ID3D11Resource* FSkyLightEnvironment::GetPanoramaResource() const { return Panorama ? Panorama->GetRawPtr() : nullptr; }
ID3D11Resource* FSkyLightEnvironment::GetEnvironmentResource() const { return EnvironmentCube ? EnvironmentCube->GetRawPtr() : nullptr; }
ID3D11Resource* FSkyLightEnvironment::GetIrradianceResource() const { return IrradianceCube ? IrradianceCube->GetRawPtr() : nullptr; }
