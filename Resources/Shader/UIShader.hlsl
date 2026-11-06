#pragma pack_matrix(row_major)

cbuffer UIParams : register(b0)
{
    float2 InvViewportSize; // 1 / 렌더 타깃 픽셀 크기
    float PxRange; // MSDF distance range
    float Padding;
};

struct VSInput
{
    float2 Position : POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
    float Mode : TEXCOORD1;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
    nointerpolation float Mode : TEXCOORD1;
};

PSInput mainVS(VSInput Input)
{
    PSInput Output;
    // 픽셀 좌표(좌상단 원점) → NDC(가운데 원점, Y 위로)
    float2 Ndc = Input.Position * InvViewportSize * 2.0f - 1.0f;
    Output.Position = float4(Ndc.x, -Ndc.y, 0.0f, 1.0f);
    Output.UV = Input.UV;
    Output.Color = Input.Color;
    Output.Mode = Input.Mode;
    return Output;
}

Texture2D UITexture : register(t0);
SamplerState LinearSampler : register(s0);

float Median(float3 msdf)
{
    return max(min(msdf.r, msdf.g), min(max(msdf.r, msdf.g), msdf.b));
}

float4 mainPS(PSInput Input) : SV_TARGET
{
    // 미분은 분기 밖에서 계산한다.
    float2 UVWidth = fwidth(Input.UV);

    if (Input.Mode < 0.5f)          // Solid
        return Input.Color;

    float4 Sample = UITexture.Sample(LinearSampler, Input.UV);
    if (Input.Mode < 1.5f)          // Texture
        return Sample * Input.Color;

    // Text (MSDF)
    float TexWidth, TexHeight;
    UITexture.GetDimensions(TexWidth, TexHeight);
    float2 UnitRange = float2(PxRange, PxRange) / float2(TexWidth, TexHeight);
    float ScreenPxRange = max(0.5f * dot(UnitRange, 1.0f / UVWidth), 1.0f);
    float Opacity = saturate(ScreenPxRange * (Median(Sample.rgb) - 0.5f) + 0.5f);
    return float4(Input.Color.rgb, Input.Color.a * Opacity);
}