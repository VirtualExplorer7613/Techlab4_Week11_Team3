#ifndef LIGHT_EVALUATION_INCLUDED
#define LIGHT_EVALUATION_INCLUDED

#include "LightData.hlsli"

#define SHADOW_FILTER_PCSS 4u

// hwang의 Ambient/Lambert/Blinn-Phong 식을 Forward와 Deferred가 공유한다.
struct FLightResult
{
    float3 Diffuse;
    float3 Specular;
};

struct FShadowCoordinates
{
    float Depth;
    float2 CenterUV;
    float2 DepthSlope;
    float2 MinUV;
    float2 MaxUV;
    float TexelSlopeBias; // 수신면 기울기 × 텍셀 절반 (깊이 0~1 단위)
};

// Best-candidate Poisson samples within the unit disk.
#define POISSON_COUNT 16
static const float2 PoissonDisk[POISSON_COUNT] =
{
    float2(0.758417f, 0.604537f),
    float2(0.666262f, -0.501792f),
    float2(0.044159f, 0.940217f),
    float2(-0.618833f, 0.442616f),
    float2(0.276293f, 0.651964f),
    float2(-0.399537f, -0.290897f),
    float2(-0.104335f, -0.698866f),
    float2(0.260315f, -0.652012f),
    float2(0.129753f, 0.215359f),
    float2(0.333606f, -0.138314f),
    float2(0.484882f, 0.283357f),
    float2(-0.074304f, -0.153791f),
    float2(-0.255648f, 0.191645f),
    float2(-0.649947f, -0.709589f),
    float2(0.838194f, 0.199567f),
    float2(-0.354359f, 0.686538f)
};

float InterleavedGradientNoise(float2 pixelPos)
{
    return frac(52.9829189f * frac(dot(pixelPos, float2(0.06711056f, 0.00583715f))));
}

float CalculateDistanceAttenuation(float DistanceSquared, float RadiusSquared)
{
    if (RadiusSquared <= 0.0f || DistanceSquared >= RadiusSquared || DistanceSquared < 1.0e-8f)
        return 0.0f;
    float RadiusFade = saturate(1.0f - DistanceSquared / RadiusSquared);
    return RadiusFade * RadiusFade / (1.0f + DistanceSquared);
}

float CalculateConeAttenuation(float CosAngle, float CosInner, float CosOuter)
{
    if (CosAngle <= CosOuter) return 0.0f;
    return saturate((CosAngle - CosOuter) / max(CosInner - CosOuter, 1.0e-8f));
}

void AccumulateLight(inout FLightResult Result, float3 N, float3 V, float3 L,
    float3 Radiance, float3 SpecularColor, float Shininess, bool bIncludeSpecular)
{
    float NoL = saturate(dot(N, L));
    Result.Diffuse += Radiance * NoL;
    if (bIncludeSpecular && NoL > 0.0f)
    {
        float3 H = L + V;
        H *= rsqrt(max(dot(H, H), 1.0e-8f));
        Result.Specular += Radiance * SpecularColor * pow(saturate(dot(N, H)), max(Shininess, 1.0f));
    }
}

float CalculateLinearDepth(float Z, float Near, float Far)
{
    return (Near * Far) / (Far - Z * (Far - Near));
}

float2 CalculateShadowDepthSlope(float3 N, float3 Coord, float CoordW, row_major float4x4 ViewProjection)
{
    float3 W = float3(ViewProjection[0][3], ViewProjection[1][3], ViewProjection[2][3]);
    float3 Jx = (float3(ViewProjection[0][0], ViewProjection[1][0], ViewProjection[2][0]) - Coord.x * W) / CoordW;
    float3 Jy = (float3(ViewProjection[0][1], ViewProjection[1][1], ViewProjection[2][1]) - Coord.y * W) / CoordW;
    float3 Jz = (float3(ViewProjection[0][2], ViewProjection[1][2], ViewProjection[2][2]) - Coord.z * W) / CoordW;
    float3 XBasis = cross(Jy, N);
    float3 YBasis = cross(N, Jx);
    float Determinant = dot(Jx, XBasis);
    return abs(Determinant) > 1.0e-12f
        ? float2(dot(Jz, XBasis), dot(Jz, YBasis)) / Determinant : 0.0f;
}

float PCF(uint SampleCount, FShadowCoordinates Coordinates, float3 PixelPosition, float3 P, float2 FilterRadius, float Bias, Texture2DArray Atlas, uint AtlasIndex, SamplerComparisonState ShadowSampler)
{
    float Result = 1.0;

    SampleCount = min(SampleCount, POISSON_COUNT);

    float S, C;
    sincos(InterleavedGradientNoise(PixelPosition.xy) * 6.28318530718f, S, C);

    int ValidSample = 0;
    float Sum = 0.0;
    // 펼치지 않는다: 이 함수는 라이트 종류·cascade 블렌딩마다 여러 번 인라인되어, 펼치면 Release(최적화) 컴파일이 수십 초 걸린다.
    [loop]
    for (uint i = 0; i < SampleCount; i++)
    {
        float2 Rotated = float2(C * PoissonDisk[i].x - S * PoissonDisk[i].y, S * PoissonDisk[i].x + C * PoissonDisk[i].y);
        float2 Offset = Rotated * FilterRadius;
        float2 UV = Coordinates.CenterUV + Offset;

        if (any(UV < Coordinates.MinUV) || any(UV > Coordinates.MaxUV))
        {
            continue;
        }

        // 탭 위치에서의 수신면 깊이로 비교한다. 중심 깊이로 비교하면 비스듬한 면이 자기 자신을 가린다고 판정되어,
        // 필터 반경(PCF 옵션)이 커질수록 전체가 어두워진다.
        float ReceiverDepthAtSample = Coordinates.Depth + dot(Coordinates.DepthSlope, Offset);
        Sum += Atlas.SampleCmpLevelZero(ShadowSampler, float3(UV, AtlasIndex), ReceiverDepthAtSample - Bias).r;
        ValidSample++;
    }

    if (ValidSample > 0)
    {
        Result = Sum / (float) ValidSample;
    }

    return Result;
}

bool CalculateShadowCoordinates(float3 P, float3 N, FLightShadowRenderData Shadow, uint AtlasWidth, uint AtlasHeight, out FShadowCoordinates Coordinates)
{
    Coordinates = (FShadowCoordinates)0;
    float4 ClipPosition = mul(float4(P, 1.0), Shadow.ViewProjection);
    float3 ShadowPosition = ClipPosition.xyz / ClipPosition.w;

    float CenterU = ShadowPosition.x * 0.5 + 0.5;
    CenterU = (Shadow.AtlasRect.x + CenterU * Shadow.AtlasRect.z) / AtlasWidth;

    float CenterV = -ShadowPosition.y * 0.5 + 0.5;
    CenterV = (Shadow.AtlasRect.y + CenterV * Shadow.AtlasRect.w) / AtlasHeight;

    float2 CenterUV = float2(CenterU, CenterV);

    float2 NdcSlope = CalculateShadowDepthSlope(N, ShadowPosition, ClipPosition.w, Shadow.ViewProjection);
    float2 DepthSlope = NdcSlope * float2(2.0f * AtlasWidth / Shadow.AtlasRect.z,
                                        -2.0f * AtlasHeight / Shadow.AtlasRect.w);

    if (ClipPosition.w <= 0.0 || ShadowPosition.x < -1.0 || ShadowPosition.x > 1.0 || ShadowPosition.y < -1.0 || ShadowPosition.y > 1.0 || ShadowPosition.z < 0 || ShadowPosition.z > 1)
    {
        return false;
    }

    float2 AtlasSize = float2(AtlasWidth, AtlasHeight);
    float2 MinUV = float2(Shadow.AtlasRect.xy + 0.5) / AtlasSize;
    float2 MaxUV = float2(Shadow.AtlasRect.xy + Shadow.AtlasRect.zw - 0.5) / AtlasSize;

    CenterUV = clamp(CenterUV, MinUV, MaxUV);

    Coordinates.Depth = ShadowPosition.z;
    Coordinates.CenterUV = CenterUV;
    Coordinates.DepthSlope = DepthSlope;
    Coordinates.MinUV = MinUV;
    Coordinates.MaxUV = MaxUV;
    // 비교 샘플러는 텍셀 중심의 깊이와 비교하므로, 수신점이 텍셀 안에서 최대 반 칸 떨어진 만큼 깊이 오차가 생긴다.
    Coordinates.TexelSlopeBias = 0.5f * dot(abs(DepthSlope), 1.0f / AtlasSize);

    return true;
}

// 그림자 뷰에서 수신점 위치의 텍셀 하나가 덮는 월드 크기.
// 행렬 첫 열의 길이 = 정사영은 2/폭, 원근은 1/tan(반각). 원근은 거리에 비례해 텍셀이 커진다.
float ShadowTexelWorldSize(FShadowCoordinates Coordinates, FLightShadowRenderData Shadow, bool bPerspective)
{
    const float XScale = length(float3(Shadow.ViewProjection[0][0], Shadow.ViewProjection[1][0], Shadow.ViewProjection[2][0]));
    const float Distance = bPerspective ? CalculateLinearDepth(Coordinates.Depth, Shadow.Near, Shadow.Far) : 1.0f;
    return 2.0f * Distance / max(XScale * Shadow.AtlasRect.z, 1.0e-6f);
}

// 월드 거리를 그림자 뷰 깊이(0~1) 차이로 바꾼다.
// - 방향광(정사영): 깊이가 선형 → 깊이 1 = (Far - Near) 월드
// - Spot/Point(원근): 깊이가 1/거리에 비례 → dDepth/d거리 = Near*Far / ((Far-Near) * 거리²)
float ShadowWorldToDepth(float WorldDistance, FShadowCoordinates Coordinates, FLightShadowRenderData Shadow, bool bPerspective)
{
    if (!bPerspective)
        return WorldDistance / max(Shadow.Far - Shadow.Near, 1.0e-4f);
    const float Distance = CalculateLinearDepth(Coordinates.Depth, Shadow.Near, Shadow.Far);
    return WorldDistance * Shadow.Near * Shadow.Far / max((Shadow.Far - Shadow.Near) * Distance * Distance, 1.0e-8f);
}

// 깊이 비교 bias (그림자 뷰 깊이 0~1 단위). 모든 라이트, 모든 필터가 이 값을 쓴다.
// 정규화 깊이 고정값은 깊이 범위·원근 거리·해상도(Quality)에 따라 월드 크기가 크게 달라져
// 낮은 해상도에서는 acne(줄무늬), 넓은 깊이 범위나 먼 거리에서는 peter panning이 생긴다.
// 그래서 월드 거리와 텍셀 크기로 정하고 깊이로 환산한다.
float CalculateShadowDepthBias(FShadowCoordinates Coordinates, FLightShadowRenderData Shadow, bool bPerspective)
{
    const float TexelWorld = ShadowTexelWorldSize(Coordinates, Shadow, bPerspective);
    // 1) 상수 항: 최소 거리 + 텍셀의 1/4. 해상도를 낮추면 텍셀과 함께 커진다.
    const float ConstantBias = ShadowWorldToDepth(0.01f + 0.25f * TexelWorld, Coordinates, Shadow, bPerspective);
    // 2) 수신면 기울기 항: 텍셀 중심과 수신점 사이의 깊이 차이. 빛과 거의 평행한 면은 기울기가 끝없이 커지므로
    //    텍셀 2칸 분량으로 제한한다(그런 면은 N·L이 0에 가까워 어차피 어둡다).
    const float SlopeBias = min(Coordinates.TexelSlopeBias, ShadowWorldToDepth(2.0f * TexelWorld, Coordinates, Shadow, bPerspective));
    return ConstantBias + SlopeBias;
}

bool FindAverageBlockerDepth(float3 PixelPosition, FShadowCoordinates Coordinates, float2 SearchRadiusUV, float DepthBias, FLightShadowRenderData Shadow, bool bPerspective, Texture2DArray Atlas, SamplerState PointSampler, out float AverageBlockerDepth)
{
    AverageBlockerDepth = 0.0;

    int BlockerCount = 0;
    float BlockerDepthSum = 0.0f;

    // 무조건 중앙 샘플 포함
    float CenterDepth = Atlas.SampleLevel(PointSampler, float3(Coordinates.CenterUV, Shadow.AtlasIndex), 0).r;
    if (CenterDepth < Coordinates.Depth - DepthBias)
    {
        BlockerDepthSum += bPerspective ? CalculateLinearDepth(CenterDepth, Shadow.Near, Shadow.Far) : CenterDepth;
        BlockerCount++;
    }

    float S, C;
    sincos(InterleavedGradientNoise(PixelPosition.xy) * 6.28318530718f, S, C);

    // PCF와 같은 이유로 펼치지 않는다.
    [loop]
    for (uint i = 0; i < POISSON_COUNT; i++)
    {
        float2 Rotated = float2(C * PoissonDisk[i].x - S * PoissonDisk[i].y, S * PoissonDisk[i].x + C * PoissonDisk[i].y);
        float2 Offset = Rotated * SearchRadiusUV;
        float2 UV = Coordinates.CenterUV + Offset;

        if (any(UV < Coordinates.MinUV) || any(UV > Coordinates.MaxUV))
        {
            continue;
        }

        // PCF와 같이 탭 위치의 수신면 깊이로 비교한다. 아니면 비스듬한 바닥 자체가 blocker로 잡혀 반그림자가 넓어진다.
        float ReceiverDepthAtSample = Coordinates.Depth + dot(Coordinates.DepthSlope, Offset);
        float SampledDepth = Atlas.SampleLevel(PointSampler, float3(UV, Shadow.AtlasIndex), 0).r;
        if (SampledDepth < ReceiverDepthAtSample - DepthBias)
        {
            BlockerDepthSum += bPerspective ? CalculateLinearDepth(SampledDepth, Shadow.Near, Shadow.Far) : SampledDepth;
            BlockerCount++;
        }
    }

    if (BlockerCount == 0)
    {
        return false;
    }

    AverageBlockerDepth = BlockerDepthSum / float(BlockerCount);
    return true;
}

float CalculateDirectionalShadowPCSS(float3 PixelPosition, float3 P, float3 N, FLightRenderParameters Light, FLightShadowRenderData Shadow, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, SamplerState PointSampler, SamplerComparisonState ShadowSampler)
{
    FShadowCoordinates Coordinates;
    if (!CalculateShadowCoordinates(P, N, Shadow, AtlasWidth, AtlasHeight, Coordinates))
    {
        return 1.0f;
    }

    // 월드 거리·텍셀 크기 기준 bias (CalculateShadowDepthBias)
    const float DepthBias = CalculateShadowDepthBias(Coordinates, Shadow, false);

    float2 AtlasSize = float2(AtlasWidth, AtlasHeight);

    // Near에서 Reciever까지의 거리 연산
    float ReceiverDistanceFromNear = Coordinates.Depth * (Shadow.Far - Shadow.Near);

    // World 공간의 탐색 범위를 계산한다.
    float SearchRadiusWorld = Light.SourceSize * ReceiverDistanceFromNear;

    // VP에서 UV 변환 비율을 추출한다.
    float2 WorldToLocalUV = 0.5f * float2(
        length(float3(Shadow.ViewProjection[0][0], Shadow.ViewProjection[1][0], Shadow.ViewProjection[2][0])),
        length(float3(Shadow.ViewProjection[0][1], Shadow.ViewProjection[1][1], Shadow.ViewProjection[2][1]))
    );

    // World 공간의 탐색 반지름을 UV 공간으로 변경한다.
    float2 SearchRadiusUV = SearchRadiusWorld * WorldToLocalUV;

    // Atlas 내에서의 UV를 구한다.
    float2 LocalToAtlasUV = Shadow.AtlasRect.zw / AtlasSize;
    SearchRadiusUV *= LocalToAtlasUV;

    // Blocker를 탐색한다.
    float AverageBlockerDepth;
    if (!FindAverageBlockerDepth(PixelPosition, Coordinates, SearchRadiusUV, DepthBias, Shadow, false, Atlas, PointSampler, AverageBlockerDepth))
    {
        return 1.0f;
    }

    // Blocker의 평균 깊이와 Reciever의 평균 깊이를 통해 reciever와 blocker의 거리를 구한다.
    float RecieverDist = max(Coordinates.Depth - AverageBlockerDepth, 0.0) * (Shadow.Far - Shadow.Near);

    // 필터 반지름과 그 값을 통해 FilterRadiusUV를 구한다.
    float FilterRadiusWorld = Light.SourceSize * RecieverDist;
    float2 FilterRadiusUV = FilterRadiusWorld * WorldToLocalUV * LocalToAtlasUV;

    return PCF(POISSON_COUNT, Coordinates, PixelPosition, P, FilterRadiusUV, DepthBias, Atlas, Shadow.AtlasIndex, ShadowSampler);
}

float CalculatePointShadowPCSS(float3 PixelPosition, float3 P, float3 N, FLightRenderParameters Light, FLightShadowRenderData Shadow, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, SamplerState PointSampler, SamplerComparisonState ShadowSampler)
{
    FShadowCoordinates Coordinates;
    if (!CalculateShadowCoordinates(P, N, Shadow, AtlasWidth, AtlasHeight, Coordinates))
    {
        return 1.0f;
    }

    const float DepthBias = CalculateShadowDepthBias(Coordinates, Shadow, true); // 원근 깊이로 환산한 월드 bias

    float2 AtlasSize = float2(AtlasWidth, AtlasHeight);

    float ReceiverDepth = CalculateLinearDepth(Coordinates.Depth, Shadow.Near, Shadow.Far);

    // Near 평면 위의 발광체의 반지름을 구한다.
    float SearchRadiusAtNear = Light.SourceSize * (ReceiverDepth - Shadow.Near) / ReceiverDepth;

    // World 공간의 탐색 반지름을 UV 공간으로 변경한다.
    float SearchRadiusLocalUV = SearchRadiusAtNear / (2.0f * Shadow.Near);

    // Atlas 내에서의 UV를 구한다.
    float2 LocalToAtlasUV = Shadow.AtlasRect.zw / AtlasSize;
    float2 SearchRadiusUV = SearchRadiusLocalUV * LocalToAtlasUV;

    // Blocker를 탐색한다.
    float AverageBlockerDepth;
    if (!FindAverageBlockerDepth(PixelPosition, Coordinates, SearchRadiusUV, DepthBias, Shadow, true, Atlas, PointSampler, AverageBlockerDepth))
    {
        return 1.0f;
    }

    // 월드 반 그림자의 반지름을 구한다.
    float FilterRadiusWorld = Light.SourceSize * max(ReceiverDepth - AverageBlockerDepth, 0.0f) / max(AverageBlockerDepth, 0.0001f);

    // 반 그림자를 UV 좌표계로 변경한다.
    float FilterRadiusLocalUV = FilterRadiusWorld / (2.0f * ReceiverDepth);
    float2 FilterRadiusUV = FilterRadiusLocalUV * LocalToAtlasUV;

    return PCF(POISSON_COUNT, Coordinates, PixelPosition, P, FilterRadiusUV, DepthBias, Atlas, Shadow.AtlasIndex, ShadowSampler);
}

float CalculateSpotShadowPCSS(float3 PixelPosition, float3 P, float3 N, FLightRenderParameters Light, FLightShadowRenderData Shadow, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, SamplerState PointSampler, SamplerComparisonState ShadowSampler)
{
    FShadowCoordinates Coordinates;
    if (!CalculateShadowCoordinates(P, N, Shadow, AtlasWidth, AtlasHeight, Coordinates))
    {
        return 1.0f;
    }

    const float DepthBias = CalculateShadowDepthBias(Coordinates, Shadow, true); // 원근 깊이로 환산한 월드 bias

    float2 AtlasSize = float2(AtlasWidth, AtlasHeight);

    float ReceiverDepth = CalculateLinearDepth(Coordinates.Depth, Shadow.Near, Shadow.Far);
    float TanHalfFov = tan(acos(Light.OuterConeCos));

    // Near 평면 위의 발광체의 반지름을 구한다.
    float SearchRadiusAtNear = Light.SourceSize * (ReceiverDepth - Shadow.Near) / ReceiverDepth;

    // World 공간의 탐색 반지름을 UV 공간으로 변경한다.
    float SearchRadiusLocalUV = SearchRadiusAtNear / (2.0f * Shadow.Near * TanHalfFov);

    // Atlas 내에서의 UV를 구한다.
    float2 LocalToAtlasUV = Shadow.AtlasRect.zw / AtlasSize;
    float2 SearchRadiusUV = SearchRadiusLocalUV * LocalToAtlasUV;

    // Blocker를 탐색한다.
    float AverageBlockerDepth;
    if (!FindAverageBlockerDepth(PixelPosition, Coordinates, SearchRadiusUV, DepthBias, Shadow, true, Atlas, PointSampler, AverageBlockerDepth))
    {
        return 1.0f;
    }

    // 월드 반 그림자의 반지름을 구한다.
    float FilterRadiusWorld = Light.SourceSize * max(ReceiverDepth - AverageBlockerDepth, 0.0f) / max(AverageBlockerDepth, 0.0001f);

    // 반 그림자를 UV 좌표계로 변경한다.
    float FilterRadiusLocalUV = FilterRadiusWorld / (2.0f * ReceiverDepth * TanHalfFov);
    float2 FilterRadiusUV = FilterRadiusLocalUV * LocalToAtlasUV;

    return PCF(POISSON_COUNT, Coordinates, PixelPosition, P, FilterRadiusUV, DepthBias, Atlas, Shadow.AtlasIndex, ShadowSampler);
}

// LightType은 호출하는 곳에서 상수(LIGHT_TYPE_*)로 넘긴다. Light.Type(버퍼 값)으로 분기하면 컴파일러가
// 안 쓰는 라이트 종류의 PCSS를 지우지 못해 호출마다 세 종류가 모두 인라인되고, Release(최적화) 컴파일이 매우 느려진다.
float CalculateShadow(float3 PixelPosition, float3 P, float3 N, FLightRenderParameters Light, FLightShadowRenderData Shadow, Texture2DArray Atlas, uint AtlasWidth, uint AtlasHeight, SamplerState PointSampler, SamplerComparisonState ShadowSampler, float bReceiveShadow, uint PcfMode, uint LightType)
{
    [branch]
    if (bReceiveShadow < 0.5f)
    {
        return 1.0f;
    }

    if (PcfMode == 4)
    {
        // PCSSMode
        if (LightType == LIGHT_TYPE_DIRECTIONAL)
        {
            return CalculateDirectionalShadowPCSS(PixelPosition, P, N, Light, Shadow, Atlas, AtlasWidth, AtlasHeight, PointSampler, ShadowSampler);
        }
        else if (LightType == LIGHT_TYPE_POINT)
        {
            return CalculatePointShadowPCSS(PixelPosition, P, N, Light, Shadow, Atlas, AtlasWidth, AtlasHeight, PointSampler, ShadowSampler);
        }
        else if (LightType == LIGHT_TYPE_SPOT)
        {
            return CalculateSpotShadowPCSS(PixelPosition, P, N, Light, Shadow, Atlas, AtlasWidth, AtlasHeight, PointSampler, ShadowSampler);
        }
        return 1.0f;
    }

    FShadowCoordinates Coordinates;
    if (!CalculateShadowCoordinates(P, N, Shadow, AtlasWidth, AtlasHeight, Coordinates))
    {
        return 1.0f;
    }

    // 정규화 깊이 고정값 대신 월드 거리 기준 bias (방향광은 선형, Spot/Point는 원근 깊이로 환산)
    const float DepthBias = CalculateShadowDepthBias(Coordinates, Shadow, LightType != LIGHT_TYPE_DIRECTIONAL);

    uint SampleCount = 0;
    if (PcfMode == 0)
    {
        return Atlas.SampleCmpLevelZero(ShadowSampler, float3(Coordinates.CenterUV, Shadow.AtlasIndex), Coordinates.Depth - DepthBias).r;
    }
    else if (PcfMode == 1)
    {
        SampleCount = 8;
    }
    else if (PcfMode == 2)
    {
        SampleCount = 16;
    }
    else if (PcfMode == 3)
    {
        SampleCount = 32;
    }

    float2 TexelSize = 1.0f / float2(AtlasWidth, AtlasHeight);
    float RadiusTexels = (float) PcfMode;

    return PCF(SampleCount, Coordinates, PixelPosition, P, TexelSize * RadiusTexels, DepthBias, Atlas, Shadow.AtlasIndex, ShadowSampler);
}


// Keep the current branch's world-space shadow bias and sharpen settings around shadow's filters.
float CalculateShadow(float3 PixelPosition, float3 P, float3 N, float3 LightDirection,
    FLightRenderParameters Light, FLightShadowRenderData Shadow, Texture2DArray Atlas,
    uint AtlasWidth, uint AtlasHeight, SamplerState PointSampler,
    SamplerComparisonState ShadowSampler, float bReceiveShadow, uint PcfMode, uint LightType)
{
    float3 L = normalize(LightDirection);
    float NoL = saturate(dot(N, L));
    float SinTheta = sqrt(saturate(1.0f - NoL * NoL));
    float3 BiasedP = P + L * Shadow.Bias + N * (Shadow.SlopeBias * SinTheta);
    float Result = CalculateShadow(PixelPosition, BiasedP, N, Light, Shadow, Atlas,
        AtlasWidth, AtlasHeight, PointSampler, ShadowSampler, bReceiveShadow, PcfMode, LightType);
    return saturate((Result - 0.5f) * (1.0f + Shadow.Sharpen * 7.0f) + 0.5f);
}

void SelectCascade(float ViewDepth, float4 Distances, uint Count, float BlendRatio,
    out int Index, out float BlendT)
{
    Index = -1;
    BlendT = 0.0f;
    [loop]
    for (uint i = 0; i < Count; ++i)
    {
        if (ViewDepth < Distances[i])
        {
            Index = i;
            break;
        }
    }
    if (Index < 0)
        return;

    float Begin = Index == 0 ? 0.0f : Distances[Index - 1];
    float End = Distances[Index];
    float BlendStart = End - (End - Begin) * BlendRatio;
    BlendT = saturate((ViewDepth - BlendStart) / max(End - BlendStart, 1.0e-4f));
}
#endif
