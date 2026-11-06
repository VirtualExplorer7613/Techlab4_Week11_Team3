#pragma once

#include "Render/TextureCube.h"
#include "Render/Texture2D.h"
#include "Render/Buffer.h"
#include "Render/Shader.h"
#include "Core/EngineString.h"

class FRenderer;

// 파노라마를 GPU에서 환경 큐브맵과 확산광 irradiance 큐브맵으로 한 번 변환한다.
class FSkyLightEnvironment
{
public:
	bool Prepare(const FString& InSourcePath);
	void DispatchEnvironment(FRenderer* Renderer);
	void DispatchIrradiance(FRenderer* Renderer);
	void Complete();
	ID3D11Resource* GetPanoramaResource() const;
	ID3D11Resource* GetEnvironmentResource() const;
	ID3D11Resource* GetIrradianceResource() const;
	const FString& GetSourcePath() const { return SourcePath; }
	const FTextureCube* GetEnvironmentCube() const { return EnvironmentCube.get(); }
	const FTextureCube* GetIrradianceCube() const { return IrradianceCube.get(); }
	bool IsValid() const { return bReady; }
	bool IsPrepared() const { return Panorama && EnvironmentCube && IrradianceCube; }

private:
	FString SourcePath;
	TUniquePtr<FTextureCube> EnvironmentCube;
	TUniquePtr<FTextureCube> IrradianceCube;
	TUniquePtr<FTexture2D> Panorama;
	TUniquePtr<FComputeShader> EnvironmentCS;
	TUniquePtr<FComputeShader> IrradianceCS;
	TUniquePtr<FConstantBuffer> ConstantsCB;
	uint32 PanoramaHeight = 0;
	bool bSourceIsLinear = false;
	bool bReady = false;
};
