#pragma pack_matrix(row_major)

#include "LightData.hlsli"

Texture2D<float4> BaseColorMetallic : register(t0);
Texture2D<float4> NormalRoughness : register(t1); // w < 0이면 bReceiveShadow가 꺼진 물체 (Roughness = -w - 1)
Texture2D<float> SceneDepth : register(t2);
Texture2D<float4> ScreenShadowMask : register(t3); // 단일 광원 패스에서만 묶인다. R = 이 광원의 그림자 (1 = 빛 받음)

cbuffer DeferredLightConstants : register(b0)
{
    matrix InverseViewProjection;
    float4 CameraPosition; // W: 원근 투영 여부
    float4 CameraForward;
    float4 ViewportSize; // xy: 뷰포트 크기
    float4 AmbientColorIntensity;
    uint4 LightCounts; // x: 포인트 개수, y: 스포트 개수, z: 방향광 개수, w: 렉트 개수
    uint4 LightOffsets; // SceneLights에서 각 종류가 시작되는 위치, 순서는 위와 같음
    float4 SkyColorIntensity;
    // 0: 그림자 없는 광원 전부 + Ambient/Sky (ShadowMapChannel이 있는 광원은 건너뜀)
    // 1: SingleLightIndex 광원 하나만 ScreenShadowMask를 곱해 가산 블렌드로 더한다 (언리얼 RenderLights)
    uint bSingleLightPass;
    uint SingleLightIndex;
    uint2 LightPassPadding;
};

StructuredBuffer<FLightRenderParameters> SceneLights : register(t4);

struct FPSInput
{
    float4 Position : SV_POSITION;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

void AccumulateLocalLight(inout float3 Diffuse, inout float3 Specular,
    float3 Albedo, float Metallic, float Roughness,
    float3 N, float3 V, float3 L, float3 Radiance)
{
    float NdotL = saturate(dot(N, L));
    if (NdotL <= 0.0f) return;

    Diffuse += Albedo * (1.0f - saturate(Metallic)) * Radiance * NdotL;

    float3 H = L + V;
    float HLengthSquared = dot(H, H);
    if (HLengthSquared <= 1.0e-8f) return;
    H *= rsqrt(HLengthSquared);

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo, saturate(Metallic));
    float Gloss = 1.0f - saturate(Roughness);
    float Shininess = lerp(2.0f, 128.0f, Gloss * Gloss);
    Specular += F0 * Radiance * pow(saturate(dot(N, H)), Shininess) * NdotL;
}

// 광원 하나의 기여 (그림자 제외). Directional은 확산광만 쓴다.
float3 EvaluateLight(FLightRenderParameters Light, float3 WorldPosition, float3 N, float3 V,
    float3 Albedo, float Metallic, float Roughness)
{
    if (Light.Intensity <= 0.0f) return 0.0f;

    if (Light.Type == LIGHT_TYPE_DIRECTIONAL)
    {
        const float3 LightDir = normalize(Light.Direction);
        return Albedo * Light.Color * Light.Intensity * saturate(dot(N, -LightDir));
    }

    float3 Diffuse = 0.0f;
    float3 Specular = 0.0f;

    if (Light.Type == LIGHT_TYPE_POINT)
    {
        float3 ToLight = Light.Position - WorldPosition;
        float DistanceSquared = dot(ToLight, ToLight);
        float RadiusSquared = Light.Radius * Light.Radius;
        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
            return 0.0f;

        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
        float3 L = ToLight / Distance;
        float x = Distance / Light.Radius;
        float Attenuation = pow(saturate(1.0 - x), max(Light.FalloffExponent, 0.0f));
        float3 Radiance = Light.Color * Light.Intensity * Attenuation;
        AccumulateLocalLight(Diffuse, Specular, Albedo, Metallic, Roughness, N, V, L, Radiance);
    }
    else if (Light.Type == LIGHT_TYPE_SPOT)
    {
        float3 ToLight = Light.Position - WorldPosition;
        float DistanceSquared = dot(ToLight, ToLight);
        float RadiusSquared = Light.Radius * Light.Radius;
        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
            return 0.0f;

        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
        float3 L = ToLight / Distance;
        float ConeCos = dot(-L, normalize(Light.Direction));
        float ConeFade = pow(smoothstep(Light.OuterConeCos, Light.InnerConeCos, ConeCos),
            max(Light.FalloffExponent, 0.0001f));
        float DistanceFactor = saturate(1.0f - Distance / Light.Radius);
        float RadiusFade = DistanceFactor * DistanceFactor;
        float3 Radiance = Light.Color * Light.Intensity * ConeFade * RadiusFade;
        AccumulateLocalLight(Diffuse, Specular, Albedo, Metallic, Roughness, N, V, L, Radiance);
    }
    else if (Light.Type == LIGHT_TYPE_RECT)
    {
        float3 Up = cross(Light.Direction, Light.Right);
        float3 CenterToSurface = WorldPosition - Light.Position;

        float RectX = clamp(dot(CenterToSurface, Light.Right), -Light.HalfWidth, Light.HalfWidth);
        float RectY = clamp(dot(CenterToSurface, Up), -Light.HalfHeight, Light.HalfHeight);
        float3 ClosestPoint = Light.Position + Light.Right * RectX + Up * RectY;

        float3 ToLight = ClosestPoint - WorldPosition;
        float DistanceSquared = dot(ToLight, ToLight);
        float RadiusSquared = Light.Radius * Light.Radius;
        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
            return 0.0f;

        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
        float3 L = ToLight / Distance;

        float Attenuation = pow(saturate(1.0f - Distance / max(Light.Radius, 0.001f)), max(Light.FalloffExponent, 0.001f));
        float FrontFace = saturate(dot(-L, Light.Direction));
        if (FrontFace <= 0.0f || Attenuation <= 0.0f)
            return 0.0f;

        float BarnDoorMask = 1.0f;
        if (Light.BarnDoorLength > 0.0f)
        {
            float3 EmitterToSurface = WorldPosition - ClosestPoint;
            float EmitterDist = max(length(EmitterToSurface), 0.0001f);
            float ForwardDist = dot(EmitterToSurface, Light.Direction);
            float LateralX = abs(dot(EmitterToSurface, Light.Right));
            float LateralY = abs(dot(EmitterToSurface, Up));
            float SlackX = (ForwardDist * Light.BarnDoorSin - LateralX * Light.BarnDoorCos) / EmitterDist;
            float SlackY = (ForwardDist * Light.BarnDoorSin - LateralY * Light.BarnDoorCos) / EmitterDist;
            BarnDoorMask = smoothstep(-0.05f, 0.0f, SlackX) * smoothstep(-0.05f, 0.0f, SlackY);
        }

        float3 Radiance = Light.Color * Light.Intensity * Attenuation * FrontFace * BarnDoorMask;
        AccumulateLocalLight(Diffuse, Specular, Albedo, Metallic, Roughness, N, V, L, Radiance);
    }
    return Diffuse + Specular;
}

// 그림자 없는 광원만 더한다. 그림자 광원은 단일 광원 패스가 마스크를 곱해 따로 더한다.
float3 AccumulateUnshadowedLights(uint Offset, uint Count, float3 WorldPosition, float3 N, float3 V,
    float3 Albedo, float Metallic, float Roughness)
{
    float3 Result = 0.0f;
    [loop]
    for (uint i = 0; i < Count; ++i)
    {
        FLightRenderParameters Light = SceneLights[Offset + i];
        if (Light.ShadowMapChannel != INVALID_SHADOW_CHANNEL) continue;
        Result += EvaluateLight(Light, WorldPosition, N, V, Albedo, Metallic, Roughness);
    }
    return Result;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    int2 Pixel = int2(Input.Position.xy);
    float Depth = SceneDepth.Load(int3(Pixel, 0));
    if (Depth >= 1.0f)
        discard; // 배경은 먼저 그린 스카이박스를 유지한다.

    float4 MaterialData = BaseColorMetallic.Load(int3(Pixel, 0));
    float4 NormalData = NormalRoughness.Load(int3(Pixel, 0));
    float3 Albedo = MaterialData.rgb;
    float Metallic = MaterialData.a;
    // GBuffer가 bReceiveShadow를 w 부호에 넣는다 (StaticMeshGBufferShader.hlsl)
    const bool bReceiveShadow = NormalData.a >= 0.0f;
    float Roughness = bReceiveShadow ? NormalData.a : -NormalData.a - 1.0f;
    float3 N = NormalData.xyz * rsqrt(max(dot(NormalData.xyz, NormalData.xyz), 1.0e-8f));

    float2 UV = (float2(Pixel) + 0.5f) / ViewportSize.xy;
    float2 NDC = float2(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f);
    float4 WorldH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
    if (!isfinite(WorldH.w) || abs(WorldH.w) < 1.0e-6f)
        return 0.0f;
    float3 WorldPosition = WorldH.xyz / WorldH.w;

    // 직교 투영의 시선 광선은 평행하므로 시선 방향이 위치에 따라 달라지지 않는다.
    float3 ToCamera = CameraPosition.w > 0.5f
        ? CameraPosition.xyz - WorldPosition : -CameraForward.xyz;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));

    if (bSingleLightPass)
    {
        // 가산 블렌드로 이 광원만 더한다. 알파는 블렌드 상태가 기존 값을 유지한다.
        const float Shadow = bReceiveShadow ? ScreenShadowMask.Load(int3(Pixel, 0)).r : 1.0f;
        return float4(EvaluateLight(SceneLights[SingleLightIndex], WorldPosition, N, V, Albedo, Metallic, Roughness) * Shadow, 0.0f);
    }

    float3 AmbientColor = AmbientColorIntensity.rgb * max(AmbientColorIntensity.a, 0.0f);
    float3 BaseLighting = AmbientColor;
    if (SkyColorIntensity.a > 0.0f)
        BaseLighting += SkyColorIntensity.rgb * SkyColorIntensity.a;
    float3 Lighting = Albedo * BaseLighting;

    Lighting += AccumulateUnshadowedLights(LightOffsets.z, LightCounts.z, WorldPosition, N, V, Albedo, Metallic, Roughness);
    Lighting += AccumulateUnshadowedLights(LightOffsets.x, LightCounts.x, WorldPosition, N, V, Albedo, Metallic, Roughness);
    Lighting += AccumulateUnshadowedLights(LightOffsets.y, LightCounts.y, WorldPosition, N, V, Albedo, Metallic, Roughness);
    Lighting += AccumulateUnshadowedLights(LightOffsets.w, LightCounts.w, WorldPosition, N, V, Albedo, Metallic, Roughness);
    return float4(Lighting, 1.0f);
}



// GGX 미세면 정반사와 에너지를 보존하는 Lambert 확산광.
//float3 EvaluateSurfaceBRDF(float3 Albedo, float Metallic, float Roughness,
//    float3 N, float3 V, float3 L)
//{
//    const float PI = 3.14159265359f;
//    float NoL = saturate(dot(N, L));
//    float NoV = saturate(dot(N, V));
//    if (NoL <= 0.0f || NoV <= 0.0f) return 0.0f;

//    float3 HalfVector = V + L;
//    float HalfLengthSquared = dot(HalfVector, HalfVector);
//    if (HalfLengthSquared < 1.0e-8f) return 0.0f;
//    float3 H = HalfVector * rsqrt(HalfLengthSquared);
//    float NoH = saturate(dot(N, H));
//    float VoH = saturate(dot(V, H));
//    Metallic = saturate(Metallic);
//    Roughness = clamp(Roughness, 0.045f, 1.0f);

//    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo, Metallic);
//    float3 F = F0 + (1.0f - F0) * pow(1.0f - VoH, 5.0f);

//    float Alpha = Roughness * Roughness;
//    float AlphaSquared = Alpha * Alpha;
//    float Denominator = NoH * NoH * (AlphaSquared - 1.0f) + 1.0f;
//    float D = AlphaSquared / max(PI * Denominator * Denominator, 1.0e-12f);
//    float GL = 2.0f * NoL /
//        (NoL + sqrt(AlphaSquared + (1.0f - AlphaSquared) * NoL * NoL));
//    float GV = 2.0f * NoV /
//        (NoV + sqrt(AlphaSquared + (1.0f - AlphaSquared) * NoV * NoV));
//    float3 Specular = D * GL * GV / max(4.0f * NoL * NoV, 1.0e-6f) * F;
//    float3 Diffuse = (1.0f - F) * (1.0f - Metallic) * Albedo / PI;
//    return (Diffuse + Specular) * NoL;
//}


//float4 mainPS(FPSInput Input) : SV_Target
//{
//    int2 Pixel = int2(Input.Position.xy);
//    float Depth = SceneDepth.Load(int3(Pixel, 0));
//    if (Depth >= 1.0f)
//        discard; // 배경은 먼저 그린 스카이박스를 유지한다.

//    float4 MaterialData = BaseColorMetallic.Load(int3(Pixel, 0));
//    float4 NormalData = NormalRoughness.Load(int3(Pixel, 0));
//    float3 Albedo = MaterialData.rgb;
//    float Metallic = MaterialData.a;
//    float Roughness = NormalData.a;
//    float3 N = NormalData.xyz * rsqrt(max(dot(NormalData.xyz, NormalData.xyz), 1.0e-8f));

//    // 첫 드로우는 기본 조명을 HDR에 쓰고, 두 번째 드로우는 포인트 라이트만 더한다.
//    if (ViewportLightCount.w < 0.5f)
//    {
//        const float3 LightDir = normalize(float3(0.5f, 0.5f, -1.0f));
//        const float3 AmbientColor = float3(0.5f, 0.5f, 0.5f);
//        const float3 LightColor = float3(0.5f, 0.5f, 0.5f);
//        float NdotL = saturate(dot(N, -LightDir));
//        return float4(Albedo * (AmbientColor + LightColor * NdotL), 1.0f);
//    }

//    float2 UV = (float2(Pixel) + 0.5f) / ViewportLightCount.xy;
//    float2 NDC = float2(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f);
//    float4 WorldH = mul(float4(NDC, Depth, 1.0f), InverseViewProjection);
//    if (!isfinite(WorldH.w) || abs(WorldH.w) < 1.0e-6f)
//        return 0.0f;
//    float3 WorldPosition = WorldH.xyz / WorldH.w;
//    // 직교 투영의 시선 광선은 평행하므로 시선 방향이 위치에 따라 달라지지 않는다.
//    float3 ToCamera = CameraPosition.w > 0.5f
//        ? CameraPosition.xyz - WorldPosition : -CameraForward.xyz;
//    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));

//    float3 PointLighting = 0.0f;

//    [loop]
//    for (uint Index = 0; Index < (uint) ViewportLightCount.z; ++Index)
//    {
//        FPointLightData Light = PointLights[Index];
//        float3 ToLight = Light.PositionRadius.xyz - WorldPosition;
//        float DistanceSquared = dot(ToLight, ToLight);
//        float RadiusSquared = Light.PositionRadius.w * Light.PositionRadius.w;
//        if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
//            continue;

//        float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
//        float3 L = ToLight / Distance;
//        float RadiusFade = saturate(1.0f - DistanceSquared / RadiusSquared);
//        float Attenuation = RadiusFade * RadiusFade / (1.0f + DistanceSquared);
//        float3 SurfaceResponse = EvaluateSurfaceBRDF(Albedo, Metallic, Roughness, N, V, L);
//        float3 LightRadiance = Light.ColorIntensity.rgb * Light.ColorIntensity.a * Attenuation;
//        PointLighting += SurfaceResponse * LightRadiance;
//    }

//    return float4(PointLighting, 0.0f);
//}

