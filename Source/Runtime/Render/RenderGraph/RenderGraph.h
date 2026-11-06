#pragma once

#include "Core/Types.h"
#include "Math/Rect.h"
#include "Containers/Array.h"
#include "Render/RenderTypes.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

class FTexture2D;
class FRenderGraphBuilder;

// 핸들은 이 그래프의 Execute가 끝날 때까지만 유효하다. 텍스처의 소유권은 갖지 않는다.
struct FRenderGraphTextureRef
{
	static constexpr uint32 InvalidIndex = UINT32_MAX;
	uint32 Index = InvalidIndex;
	const FRenderGraphBuilder* Owner = nullptr;
	explicit operator bool() const { return Owner && Index != InvalidIndex; }
};

enum class ERenderGraphLoadAction : uint8
{
	Load,
	Clear,
};

struct FRenderGraphColorAttachment // 패스에 필요한 ColorTextrue 핸들
{
	FRenderGraphTextureRef Texture;
	ERenderGraphLoadAction LoadAction = ERenderGraphLoadAction::Load;
	std::array<float, 4> ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
};

struct FRenderGraphDepthAttachment // 패스에 필요한 DepthTextrue 핸들
{
	FRenderGraphTextureRef Texture;
	ERenderGraphLoadAction LoadAction = ERenderGraphLoadAction::Load;
	float ClearDepth = 1.0f;
	uint8 ClearStencil = 0;
	uint32 DSVIndex = 0;
};

struct FShadowAtlasRect
{
	uint32 X = 0;
	uint32 Y = 0;
	uint32 Width = 0;
	uint32 Height = 0;
};

struct FRenderGraphRasterPassDesc // draw pass용
{
	std::string Name;
	std::vector<FRenderGraphTextureRef> ReadTextures;
	std::vector<FRenderGraphColorAttachment> ColorAttachments;
	FRenderGraphDepthAttachment DepthAttachment;
	FShadowAtlasRect Viewport{}; // 0 크기면 출력 전체. Atlas 타일은 부분 영역을 지정한다.
};

struct FRenderGraphComputePassDesc // compute pass용
{
	std::string Name;
	std::vector<ID3D11Resource*> ReadResources;
	std::vector<ID3D11Resource*> WriteResources;
};

struct FRenderGraphTextureDesc
{
	uint32 Width = 0;
	uint32 Height = 0;
	ETextureFormat Format = ETextureFormat::Unknown;
	ETextureBindFlags BindFlags = ETextureBindFlags::RenderTarget | ETextureBindFlags::ShaderResource;
};

// 1단계: 외부 텍스처와 순차 래스터 패스만 지원한다.
// 실행 함수는 RTV를 유지하고, DSV는 선언된 DepthAttachments 내에서만 전환한다.
class FRenderGraphBuilder
{
public:
	FRenderGraphBuilder() = default;
	~FRenderGraphBuilder();
	FRenderGraphBuilder(const FRenderGraphBuilder&) = delete;
	FRenderGraphBuilder& operator=(const FRenderGraphBuilder&) = delete;

	FRenderGraphTextureRef ImportTexture(FTexture2D* Texture, const char* Name);
	FRenderGraphTextureRef CreateTexture(const FRenderGraphTextureDesc& Desc, const char* Name);
	FTexture2D* Resolve(FRenderGraphTextureRef Ref) const;

	bool AddRasterPass(FRenderGraphRasterPassDesc Desc, std::function<void()> Execute);
	bool AddComputePass(FRenderGraphComputePassDesc Desc, std::function<void()> Execute);
	bool Execute();

private:
	struct FImportedTexture
	{
		FTexture2D* Texture = nullptr;
		std::string Name;
		FRenderGraphTextureDesc Desc{};
		bool bTransient = false;
	};
	struct FRasterPass
	{
		FRenderGraphRasterPassDesc Desc;
		FRenderGraphComputePassDesc ComputeDesc;
		std::function<void()> Execute;
		uint32 X = 0;
		uint32 Y = 0;
		uint32 Width = 0;
		uint32 Height = 0;
		bool bCompute = false;
	};

	bool ValidatePass(const FRenderGraphRasterPassDesc& Desc) const;

	std::vector<FImportedTexture> Textures;
	std::vector<FRasterPass> Passes;
	bool bExecuted = false;
};
