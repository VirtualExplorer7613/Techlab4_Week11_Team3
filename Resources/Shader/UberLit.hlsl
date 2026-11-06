#pragma pack_matrix(row_major)

// 셰이더 스캐너는 매크로 정의 없이 이 파일을 컴파일하므로 기본 변형에는 Lambert를 쓴다.
// 명시적인 변형은 조명 모델 또는 뷰 모드 하나만 1로 설정한다.
#if !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG) && !defined(LIGHTING_MODEL_UNLIT) && !defined(VIEW_MODE_WORLD_NORMAL)
    #define LIGHTING_MODEL_LAMBERT 1
#endif

#if (LIGHTING_MODEL_GOURAUD + LIGHTING_MODEL_LAMBERT + LIGHTING_MODEL_PHONG + LIGHTING_MODEL_UNLIT + VIEW_MODE_WORLD_NORMAL) != 1
    #error Exactly one lighting model or view mode must be enabled.
#endif


cbuffer PerObject : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
    matrix NormalMatrix; // World 행렬의 역행렬을 전치한 법선 변환 행렬
    float4 ShadowFlags;
};

cbuffer MaterialParams : register(b2)
{
    float4 BaseColor;
    float4 Emissive;
    float4 SurfaceParams; // x: 노멀맵 Y 반전, y: 예약, z: 컬러 텍스처 사용, w: 노멀맵 인코딩
    float4 UVTransform; // xy: 이동량, zw: 배율
    float4 SpecularColorShininess; // rgb: 정반사 색상, w: 광택 지수
};

Texture2D ColorTexture : register(t0);
Texture2D NormalTexture : register(t1);

SamplerState ColorSampler : register(s0);

#include "ForwardLighting.hlsli"

struct VS_IN
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
	float2 UV : TEXCOORD;
	float4 Color : COLOR;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 WorldNormal : TEXCOORD4;
    float4 WorldTangent : TEXCOORD5;
#if LIGHTING_MODEL_GOURAUD
    float3 VertexDiffuse : TEXCOORD1;
    float3 VertexSpecular : TEXCOORD2;
#elif !VIEW_MODE_WORLD_NORMAL
    float3 WorldPosition : TEXCOORD1;
    float ViewDepth : TEXCOORD2;
#endif
};

struct FUberOutput
{
    float4 LitColor : SV_Target0;
#if !VIEW_MODE_WORLD_NORMAL
    float4 NormalRoughness : SV_Target1;
#endif
};

float3 DecodeMapNormal(float3 Sample, float Encoding)
{
    if (Encoding > 1.5f)
    {
        float2 XY = Encoding > 2.5f ? Sample.xy : Sample.xy * 2.0f - 1.0f;
        return float3(XY, sqrt(saturate(1.0f - dot(XY, XY))));
    }
    return Sample * 2.0f - 1.0f;
}

// 조명과 World Normal 보기가 같은 노멀맵 해석 및 TBN 변환을 사용한다.
float3 GetShadingNormal(VS_OUT Input)
{
    float LengthSquared = dot(Input.WorldNormal, Input.WorldNormal);
    float3 N = LengthSquared > 1.0e-8f
        ? Input.WorldNormal * rsqrt(LengthSquared)
        : float3(0.0f, 0.0f, 1.0f);
    if (SurfaceParams.w > 0.5f)
    {
        float3 MapNormal = DecodeMapNormal(NormalTexture.Sample(ColorSampler, Input.UV).xyz, SurfaceParams.w);
        if (SurfaceParams.x > 0.5f) MapNormal.y = -MapNormal.y;
        float3 T = Input.WorldTangent.xyz - N * dot(N, Input.WorldTangent.xyz);
        float TangentLengthSquared = dot(T, T);
        if (TangentLengthSquared > 1.0e-8f)
        {
            T *= rsqrt(TangentLengthSquared);
            float3 B = cross(N, T) * Input.WorldTangent.w;
            float3 MappedNormal = T * MapNormal.x + B * MapNormal.y + N * MapNormal.z;
            float MappedLengthSquared = dot(MappedNormal, MappedNormal);
            if (MappedLengthSquared > 1.0e-8f)
                N = MappedNormal * rsqrt(MappedLengthSquared);
        }
    }
    return N;
}

VS_OUT Uber_VS(VS_IN Input)
{
    VS_OUT Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0f), World);
    float4 ViewPosition = mul(WorldPosition, View);
    Output.Position = mul(ViewPosition, Projection);
    Output.Color = Input.Color;
    Output.UV = Input.UV * UVTransform.zw + UVTransform.xy;

    float3 N = mul(float4(Input.Normal, 0.0f), NormalMatrix).xyz;
    Output.WorldNormal = N;
    float3 T = mul(float4(Input.Tangent.xyz, 0.0f), World).xyz;
    // CPU가 구한 로컬 공간 부호에 월드 변환의 거울 반전만 반영한다.
    float Handedness = Input.Tangent.w * (determinant((float3x3)World) < 0.0f ? -1.0f : 1.0f);
    Output.WorldTangent = float4(T, Handedness);
#if LIGHTING_MODEL_GOURAUD
    FLightResult Lit = CalculateLighting(float3(0.0f, 0.0f, 0.0f), WorldPosition.xyz, ViewPosition.x, N, true);
    Output.VertexDiffuse = Lit.Diffuse;
    Output.VertexSpecular = Lit.Specular;
#elif !VIEW_MODE_WORLD_NORMAL
    Output.WorldPosition = WorldPosition.xyz;
    Output.ViewDepth = ViewPosition.x;
#endif
    return Output;
}

FUberOutput Uber_PS(VS_OUT Input)
{
    FUberOutput Output;
    float3 ShadingNormal = GetShadingNormal(Input);
#if VIEW_MODE_WORLD_NORMAL
    Output.LitColor = float4(ShadingNormal * 0.5f + 0.5f, 1.0f);
#else
    float4 SurfaceColor;
    if (SurfaceParams.z > 0.5f)
    {
        float4 TextureColor = ColorTexture.Sample(ColorSampler, Input.UV);
        SurfaceColor = float4(pow(max(TextureColor.rgb, 0.0f), 2.2f), TextureColor.a);
    }
    else
    {
        SurfaceColor = Input.Color;
    }
    float3 Albedo = SurfaceColor.rgb * BaseColor.rgb;
    float3 Diffuse;
    float3 Specular = 0.0f;

#if LIGHTING_MODEL_GOURAUD
    Diffuse = Input.VertexDiffuse;
    Specular = Input.VertexSpecular;
#elif LIGHTING_MODEL_LAMBERT
    // 픽셀 위치와 깊이를 함께 전달한다.
    Diffuse = CalculateTiledLighting(Input.Position.xyz, Input.WorldPosition, Input.ViewDepth, ShadingNormal, false).Diffuse;
#elif LIGHTING_MODEL_PHONG
    // 같은 목록 선택 경로에서 기존 Blinn-Phong 조명을 계산한다.
    FLightResult Lit = CalculateTiledLighting(Input.Position.xyz, Input.WorldPosition, Input.ViewDepth, ShadingNormal, true);
    Diffuse = Lit.Diffuse;
    Specular = Lit.Specular;
#elif LIGHTING_MODEL_UNLIT
    // 조명·그림자를 계산하지 않고 표면 색을 그대로 낸다.
    Diffuse = 1.0f;
#endif

    Output.LitColor = float4(Albedo * Diffuse + Specular + Emissive.rgb,
        SurfaceColor.a * BaseColor.a);
    Output.NormalRoughness = float4(ShadingNormal, 0.8f);
#endif
    return Output;
}

// 현재 리소스 매니저는 .hlsl 파일을 스캔할 때 mainVS/mainPS 진입점을 찾는다.
VS_OUT mainVS(VS_IN Input) { return Uber_VS(Input); }
FUberOutput mainPS(VS_OUT Input) { return Uber_PS(Input); }
