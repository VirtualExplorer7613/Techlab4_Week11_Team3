// Shadow Mask 뷰 모드: ScreenShadowMask의 네 채널(그림자 광원 최대 4개) 중 가장 어두운 값을 회색으로 보여 준다.

Texture2D<float4> ScreenShadowMask : register(t0);

struct FVSOutput
{
    float4 Position : SV_POSITION;
};

FVSOutput mainVS(uint VertexID : SV_VertexID)
{
    FVSOutput Output;
    const float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float4 mainPS(FVSOutput Input) : SV_Target
{
    const float4 Mask = ScreenShadowMask.Load(int3(int2(Input.Position.xy), 0));
    const float Shadow = min(min(Mask.r, Mask.g), min(Mask.b, Mask.a));
    return float4(Shadow.xxx, 1.0f);
}
