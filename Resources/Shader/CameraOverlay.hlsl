cbuffer CameraOverlayConstants : register(b0)
{
    float4 FadeColor; // rgb = 색, a = FadeAmount
    float2 ViewSize;
    float VignetteIntensity; // 0 = 없음. 0.5~1.5가 보기 좋음
    float VignettePower; // 가장자리 경도
    float LetterboxAmount; // 0~1
    float LetterboxAspect; // 2.39 = 시네마스코프
    float2 Padding;
};

struct FPSInput
{
    float4 Position : SV_POSITION;
};

// ToneMapShader.hlsl과 같은 풀스크린 삼각형 (정점 버퍼 없이 Draw(3))
FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    const float2 UV = Input.Position.xy / ViewSize;
    const float ViewAspect = ViewSize.x / ViewSize.y;

    // 1) Letterbox: 보이는 높이 비율 = ViewAspect / TargetAspect, 나머지를 위아래 반씩
    const float Bar = saturate(0.5f * (1.0f - ViewAspect / LetterboxAspect)) * LetterboxAmount;
    if (UV.y < Bar || UV.y > 1.0f - Bar)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    // 2) Vignette: 화면비 보정한 중심 거리 → 원형 유지 (보정 안 하면 가로로 긴 타원)
    float2 D = (UV - 0.5f) * float2(ViewAspect, 1.0f);
    float V = pow(saturate(1.0f - dot(D, D) * VignetteIntensity), VignettePower);

    // 3) Fade + Vignette를 블렌드 식 하나로 (위 유도)
    const float a = FadeColor.a;
    const float Alpha = 1.0f - V * (1.0f - a);
    const float3 Src = (Alpha > 1e-4f) ? FadeColor.rgb * (a / Alpha) : float3(0.0f, 0.0f, 0.0f);
    return float4(Src, Alpha);
}