#pragma pack_matrix(row_major)
#include "ColorSpace.hlsli"

cbuffer Viewconstants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
    matrix NormalMatrix;
    float4 ShadowFlags;
};

cbuffer DecalConstants : register(b4)
{
    matrix WorldToDecal;
    float4 DecalColor;
    float FadeAlpha;
    float Intensity;
    float IsAdditive;
    float HasShadowDecal;
    float IsUnlit;
    float3 DecalPadding;
};

static const float4 SpecularColorShininess = float4(0.f, 0.f, 0.f, 1.f);

#define LIGHT_SHADOW_ENABLED (ShadowFlags.x > 0.5f && HasShadowDecal > 0.5f)

#include "ForwardLighting.hlsli"

struct VS_INPUT
{
    float3 p : POSITION;
    float3 n : NORMAL;
    float4 tangent : TANGENT;
    float2 t : TEXCOORD;
    float4 c : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
};

Texture2D g_txDecal : register(t0);
SamplerState g_Sample : register(s0);

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    float4 WorldPosition = mul(float4(Input.p, 1.f), World);
    Output.position = mul(WorldPosition, VP);
    Output.worldPosition = WorldPosition.xyz;
    Output.worldNormal = mul(float4(Input.n, 0.f), NormalMatrix).xyz;
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float3 DecalPosition = mul(float4(Input.worldPosition, 1.f), WorldToDecal).xyz;
    
    if (any(abs(DecalPosition) > 1.f))
    {
        discard;
    }
    
    float3 N = normalize(Input.worldNormal);
    float2 UV = float2(DecalPosition.y * 0.5f + 0.5f, 0.5f - DecalPosition.z * 0.5f);
    float4 Color = g_txDecal.Sample(g_Sample, UV);
    float DepthFade = saturate(4.f - 4.f * abs(DecalPosition.x));

    if (IsAdditive > 0.5f)
    {
        // Fake 라이트: 텍스처 알파를 감쇠 마스크로 한 번만 사용한다
        // (sRGB→Linear 변환 + 알파 재곱셈을 하면 감쇠가 f^3.2 가 되어 원이 작게 보임)
        float Mask = Color.a * DepthFade * FadeAlpha;
        return float4(DecalColor.rgb * Intensity * Mask, Mask);
    }

    Color.rgb = SRGBToLinear(Color.rgb);
    Color.a *= DepthFade * FadeAlpha;
    Color.rgb *= DecalColor.rgb * Intensity;

    if (IsUnlit < 0.5f)
    {
        FLightResult LightResult = CalculateLighting(Input.position.xyz, Input.worldPosition, 0, N, false);
        Color.rgb *= LightResult.Diffuse;
    }
    return Color;
}