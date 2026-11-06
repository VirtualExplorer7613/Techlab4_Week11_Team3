#pragma once

#include <imgui.h>
#include <Windows.h>

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

class FImGuiRenderer
{
public:
	FImGuiRenderer() = default;
	~FImGuiRenderer();

	bool Init(HWND WindowHandle, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
	void Begin();
	void End();
	void Shutdown();
	bool DrawShadowMap(ID3D11ShaderResourceView* AtlasSRV, unsigned int AtlasIndex,
		ImVec2 Size, ImVec2 UV0, ImVec2 UV1, float Contrast);
private:
	static void BindShadowPreview(const ImDrawList*, const ImDrawCmd* Command);
	static void SetShadowPreviewSlice(const ImDrawList*, const ImDrawCmd* Command);
	static void RestoreShadowPreview(const ImDrawList*, const ImDrawCmd* Command);
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ShadowPreviewPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ShadowPreviewCB;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> ShadowPreviewSampler;
	Microsoft::WRL::ComPtr<ID3D11Buffer> PreviousPSConstants;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> PreviewTextures;
};
