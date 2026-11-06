#ifndef FORWARD_LIGHTING_INCLUDED
#define FORWARD_LIGHTING_INCLUDED

#pragma pack_matrix(row_major)

#include "LightData.hlsli"
#include "LightEvaluation.hlsli"
#include "LightGridCommon.hlsli"

// 포함하는 셰이더가 먼저 선언해야 하는 것:
//   float4 ShadowFlags            (x: bReceiveShadow)
//   float4 SpecularColorShininess (rgb: 정반사 색, w: 광택 지수)
// 그림자를 읽을 조건을 바꾸려면 포함 전에 LIGHT_SHADOW_ENABLED를 정의한다.

#ifndef LIGHT_SHADOW_ENABLED
#define LIGHT_SHADOW_ENABLED (ShadowFlags.x > 0.5f)
#endif

// TODO: UberLit.hlsl에서 다음을 그대로 잘라 옮긴다.
struct FAmbientLightInfo
{
    float4 ColorIntensity; // rgb: 선형 색상, a: 광도
};

cbuffer Lighting : register(b1)
{
    FAmbientLightInfo Ambient;
    float4 CameraPosition;
    uint4 LightCounts; // x: 포인트 개수, y: 스포트 개수, z: 방향광 개수
    uint4 LightOffsets; // SceneLights에서 각 종류가 시작되는 위치, 순서는 위와 같음
    // 그림자는 광원마다 Light.ShadowMapChannel로 ScreenShadowMask(t13)에서 읽는다.
    float4 SkyColorIntensity; // rgb: Sky Light 색상, a: 세기 (0이면 비활성)
};

StructuredBuffer<FLightRenderParameters> SceneLights : register(t8);
StructuredBuffer<uint4> TileLightRanges : register(t9);
StructuredBuffer<uint> TilePointIndices : register(t10);
TextureCube<float4> SkyIrradiance : register(t11);
StructuredBuffer<uint> TileSpotIndices : register(t12);
// 언리얼 ScreenShadowMask: 그림자 광원마다 배정된 채널(R~A)에 Projection 결과가 들어 있다. 1 = 빛 받음.
Texture2D<float4> ScreenShadowMask : register(t13);

SamplerState SkySampler : register(s1);

// 광원의 그림자 (1 = 빛 받음). 채널이 없거나 이 물체의 bReceiveShadow가 꺼져 있으면 1.
// Gouraud는 채널을 받지 않으므로 정점 셰이더에서 불려도 마스크를 읽지 않는다.
float GetLightShadow(FLightRenderParameters Light, float3 PixelPosition)
{
    if (Light.ShadowMapChannel == INVALID_SHADOW_CHANNEL || !(LIGHT_SHADOW_ENABLED))
        return 1.0f;
    return ScreenShadowMask.Load(int3(int2(PixelPosition.xy), 0))[Light.ShadowMapChannel];
}


#if !VIEW_MODE_WORLD_NORMAL
float3 CalculateAmbientLight(FAmbientLightInfo Info)
{
    return Info.ColorIntensity.rgb * max(Info.ColorIntensity.a, 0.0f);
}

float3 CalculateSkyLight(float3 N)
{
    if (SkyColorIntensity.a <= 0.0f)
        return 0.0f;
    return SkyIrradiance.SampleLevel(SkySampler, N, 0).rgb *
        SkyColorIntensity.rgb * SkyColorIntensity.a;
}

void AddLight(inout FLightResult Result, float3 N, float3 V, float3 L,
    float3 Radiance, bool bIncludeSpecular)
{
    AccumulateLight(Result, N, V, L, Radiance, SpecularColorShininess.rgb,
        SpecularColorShininess.w, bIncludeSpecular);
}

void CalculateDirectionalLight(inout FLightResult Result, FLightRenderParameters Info, float3 N, float3 V, bool bIncludeSpecular)
{
    if (Info.Intensity <= 0.0f)
    {
        return;
    }
    
    float3 L = normalize(-Info.Direction);
    float3 Radiance = Info.Color * Info.Intensity;
    
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculatePointLight(inout FLightResult Result, FLightRenderParameters Info,
    float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.Position - P;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusSquared = Info.Radius * Info.Radius;
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
        return;
    float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
    // Deferred와 동일하게 Point Light의 RadiusFallOff를 반경 감쇠 지수로 사용한다.
    float Fade = pow(saturate(1.0f - Distance / Info.Radius), max(Info.FalloffExponent, 0.0f));
    if (Fade <= 0.0f)
        return;
    float3 L = ToLight / Distance;
    float3 Radiance = Info.Color * max(Info.Intensity, 0.0f) * Fade;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculateSpotLight(inout FLightResult Result, FLightRenderParameters Info, float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 ToLight = Info.Position - P;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusFade = CalculateDistanceAttenuation(DistanceSquared, Info.Radius * Info.Radius);
    if (RadiusFade <= 0.0f)
        return;
    float3 L = ToLight / sqrt(max(DistanceSquared, 1.0e-6f));
    float ConeCos = dot(-L, normalize(Info.Direction));
    float ConeFade = CalculateConeAttenuation(ConeCos, Info.InnerConeCos, Info.OuterConeCos);
    ConeFade = pow(ConeFade, max(Info.FalloffExponent, 0.0001f));

    float3 Radiance = Info.Color * max(Info.Intensity, 0.0f) * ConeFade * RadiusFade;

    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

void CalculateRectLight(inout FLightResult Result, FLightRenderParameters Info,
	float3 P, float3 N, float3 V, bool bIncludeSpecular)
{
    float3 Position = Info.Position;
    float Radius = Info.Radius;
    float3 Direction = Info.Direction;
    float BarnDoorLength = Info.BarnDoorLength;
    float3 Color = Info.Color;
    float Intensity = Info.Intensity;
    float BarnDoorCos = Info.BarnDoorCos;
    float BarnDoorSin = Info.BarnDoorSin;
    float FalloffExponent = Info.FalloffExponent;
    float3 Right = Info.Right;
    float HalfWidth = Info.HalfWidth;
    float HalfHeight = Info.HalfHeight;

    float3 Up = cross(Direction, Right);
    float3 CenterToSurface = P - Position;

    float RectX = clamp(dot(CenterToSurface, Right), -HalfWidth, HalfWidth);
    float RectY = clamp(dot(CenterToSurface, Up), -HalfHeight, HalfHeight);
    float3 ClosestPoint = Position + Right * RectX + Up * RectY;

    float3 ToLight = ClosestPoint - P;
    float DistanceSquared = dot(ToLight, ToLight);
    float RadiusSquared = Radius * Radius;
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
        return;

    float Distance = sqrt(max(DistanceSquared, 1.0e-6f));
    float3 L = ToLight / Distance;

    float Attenuation = pow(saturate(1.0f - Distance / max(Radius, 0.001f)), max(FalloffExponent, 0.001f));
    float FrontFace = saturate(dot(-L, Direction));
    if (FrontFace <= 0.0f || Attenuation <= 0.0f)
        return;

    float BarnDoorMask = 1.0f;
    if (BarnDoorLength > 0.0f)
    {
        float3 EmitterToSurface = P - ClosestPoint;
        float EmitterDist = max(length(EmitterToSurface), 0.0001f);
        float ForwardDist = dot(EmitterToSurface, Direction);
        float LateralX = abs(dot(EmitterToSurface, Right));
        float LateralY = abs(dot(EmitterToSurface, Up));
        float SlackX = (ForwardDist * BarnDoorSin - LateralX * BarnDoorCos) / EmitterDist;
        float SlackY = (ForwardDist * BarnDoorSin - LateralY * BarnDoorCos) / EmitterDist;
        BarnDoorMask = smoothstep(-0.05f, 0.0f, SlackX) * smoothstep(-0.05f, 0.0f, SlackY);
    }

    float3 Radiance = Color * max(Intensity, 0.0f) * Attenuation * FrontFace * BarnDoorMask;
    AddLight(Result, N, V, L, Radiance, bIncludeSpecular);
}

FLightResult CalculateBaseLighting(float3 PixelPosition, float3 P, float ViewDepth, float3 N, float3 V, bool bIncludeSpecular)
{
    FLightResult Result;
    Result.Diffuse = CalculateAmbientLight(Ambient) + CalculateSkyLight(N);
    Result.Specular = 0.0f;

    [loop]
    for (uint DirectionalIndex = 0; DirectionalIndex < LightCounts.z; ++DirectionalIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.z + DirectionalIndex];
        
        float Shadow = GetLightShadow(Light, PixelPosition);
            
        FLightResult DirectionalResult = (FLightResult) 0;
        CalculateDirectionalLight(DirectionalResult, Light, N, V, bIncludeSpecular);
        DirectionalResult.Diffuse *= Shadow;
        DirectionalResult.Specular *= Shadow;
         
        Result.Diffuse += DirectionalResult.Diffuse;
        Result.Specular += DirectionalResult.Specular;
    }
    
    return Result;
}

void AddAllLocalLights(inout FLightResult Result, float3 PixelPosition, float3 P, float3 N,
    float3 V, bool bIncludeSpecular)
{
    [loop]
    for (uint PointIndex = 0; PointIndex < LightCounts.x; ++PointIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.x + PointIndex];

        float Shadow = GetLightShadow(Light, PixelPosition);

        FLightResult PointResult = (FLightResult) 0;
        CalculatePointLight(PointResult, SceneLights[LightOffsets.x + PointIndex], P, N, V, bIncludeSpecular);
         
        Result.Diffuse += PointResult.Diffuse * Shadow;
        Result.Specular += PointResult.Specular * Shadow;
    }
    [loop]
    for (uint SpotIndex = 0; SpotIndex < LightCounts.y; ++SpotIndex)
    {
        FLightRenderParameters Light = SceneLights[LightOffsets.y + SpotIndex];
        
        float Shadow = GetLightShadow(Light, PixelPosition);
            
        FLightResult SpotResult = (FLightResult) 0;
        CalculateSpotLight(SpotResult, Light, P, N, V, bIncludeSpecular);

        Result.Diffuse += SpotResult.Diffuse * Shadow;
        Result.Specular += SpotResult.Specular * Shadow;
    }
	[loop]
    for (uint RectIndex = 0; RectIndex < LightCounts.w; ++RectIndex)
    {
        CalculateRectLight(Result, SceneLights[LightOffsets.w + RectIndex], P, N, V, bIncludeSpecular);
    }
}

FLightResult CalculateLighting(float3 PixelPosition, float3 P, float ViewDepth, float3 Normal, bool bIncludeSpecular)
{
    float3 N = normalize(Normal);
    float3 ToCamera = CameraPosition.xyz - P;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));
    FLightResult Result = CalculateBaseLighting(PixelPosition, P, ViewDepth, N, V, bIncludeSpecular);
    AddAllLocalLights(Result, PixelPosition, P, N, V, bIncludeSpecular);
    return Result;
}

// 타일 목록의 인덱스는 kim의 공유 SceneLights 안에서 종류별 시작 위치에 더한다.
FLightResult CalculateTiledLighting(float3 PixelPosition, float3 P, float ViewDepth, float3 Normal, bool bIncludeSpecular)
{
    float3 N = normalize(Normal);
    float3 ToCamera = CameraPosition.xyz - P;
    float3 V = ToCamera * rsqrt(max(dot(ToCamera, ToCamera), 1.0e-8f));
    FLightResult Result = CalculateBaseLighting(PixelPosition, P, ViewDepth, N, V, bIncludeSpecular);

    if (GridMode == LIGHT_GRID_MODE_OFF)
    {
        // Off는 업로드된 전체 로컬 광원을 평가한다.
        AddAllLocalLights(Result, PixelPosition, P, N, V, bIncludeSpecular);
    }
    else
    {
        // SV_POSITION.xy는 픽셀 위치, z는 현재 표면의 DeviceZ다.
        uint CellIndex = GetLightGridCellIndex(uint2(PixelPosition.xy), PixelPosition.z);
        uint4 Range = TileLightRanges[CellIndex];
        bool UsePointList = Range.x != 0xffffffffu;
        bool UseSpotList = Range.z != 0xffffffffu;
        uint PointCount = UsePointList ? Range.y : LightCounts.x;
        uint SpotCount = UseSpotList ? Range.w : LightCounts.y;

        // 예약 실패한 종류만 전체 배열을 평가하는 기존 fallback을 유지한다.
        [loop]
        for (uint PointListIndex = 0; PointListIndex < PointCount; ++PointListIndex)
        {
            uint LightIndex = UsePointList ? TilePointIndices[Range.x + PointListIndex] : PointListIndex;
            
            FLightRenderParameters Light = SceneLights[LightOffsets.x + LightIndex];

            float Shadow = GetLightShadow(Light, PixelPosition);
            
            FLightResult PointResult = (FLightResult) 0;
            CalculatePointLight(PointResult, Light, P, N, V, bIncludeSpecular);
         
            Result.Diffuse += PointResult.Diffuse * Shadow;
            Result.Specular += PointResult.Specular * Shadow;
        }

        [loop]
        for (uint SpotListIndex = 0; SpotListIndex < SpotCount; ++SpotListIndex)
        {
            uint LightIndex = UseSpotList ? TileSpotIndices[Range.z + SpotListIndex] : SpotListIndex;
            
            FLightRenderParameters Light = SceneLights[LightOffsets.y + LightIndex];
            
            float Shadow = GetLightShadow(Light, PixelPosition);
            
            FLightResult SpotResult = (FLightResult) 0;
            CalculateSpotLight(SpotResult, Light, P, N, V, bIncludeSpecular);
            
            Result.Diffuse += SpotResult.Diffuse * Shadow;
            Result.Specular += SpotResult.Specular * Shadow;
        }

		[loop]
        for (uint RectIndex = 0; RectIndex < LightCounts.w; ++RectIndex)
        {
            CalculateRectLight(Result, SceneLights[LightOffsets.w + RectIndex], P, N, V, bIncludeSpecular);
        }
    }
    return Result;
}

#endif

#endif