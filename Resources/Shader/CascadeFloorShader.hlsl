#pragma pack_matrix(row_major)
cbuffer CascadeFloorConstants : register(b0)
{
    float4x4 InverseViewProjection;
    float4x4 ViewProjection;
    float4x4 View;
    float4 CascadeShadowDistances;
    float4 Viewport; // width, height
    uint CascadeCount;
    uint3 Padding;
};

Texture2D<float> SceneDepth : register(t0);

struct VSOutput
{
    float4 Position : SV_Position;
};

VSOutput mainVS(uint VertexID : SV_VertexID)
{
    static const float2 Positions[6] = {
        float2(-1, 1), float2(1, 1), float2(-1, -1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };
    VSOutput Output;
    Output.Position = float4(Positions[VertexID], 0, 1);
    return Output;
}

float4 mainPS(VSOutput Input) : SV_Target
{
    int2 Pixel = int2(Input.Position.xy);
    float2 UV = (float2(Pixel) + 0.5f) / Viewport.xy;
    float2 NDC = float2(UV.x * 2.0f - 1.0f, 1.0f - UV.y * 2.0f);
    float4 NearH = mul(float4(NDC, 0.0f, 1.0f), InverseViewProjection);
    float4 FarH = mul(float4(NDC, 1.0f, 1.0f), InverseViewProjection);
    if (abs(NearH.w) < 1.0e-6f || abs(FarH.w) < 1.0e-6f)
        discard;
    float3 Near = NearH.xyz / NearH.w;
    float3 Far = FarH.xyz / FarH.w;
    float DeltaZ = Far.z - Near.z;
    if (abs(DeltaZ) < 1.0e-6f)
        discard;
    float T = -Near.z / DeltaZ;
    if (T < 0.0f || T > 1.0f)
        discard;

    float3 Floor = lerp(Near, Far, T);
    Floor.z = 0.0f;
    float4 CameraClip = mul(float4(Floor, 1.0f), ViewProjection);
    if (CameraClip.w <= 0.0f)
        discard;
    float FloorDepth = CameraClip.z / CameraClip.w;
    float SurfaceDepth = SceneDepth.Load(int3(Pixel, 0));
    if (FloorDepth < 0.0f || FloorDepth > 1.0f || SurfaceDepth + 1.0e-4f < FloorDepth)
        discard;
    float3 Surface = Floor;
    if (SurfaceDepth < 1.0f)
    {
        float4 SurfaceH = mul(float4(NDC, SurfaceDepth, 1.0f), InverseViewProjection);
        if (abs(SurfaceH.w) < 1.0e-6f)
            discard;
        Surface = SurfaceH.xyz / SurfaceH.w;
    }
    float ViewDepth = mul(float4(Surface, 1.0f), View).x;

    static const float3 Colors[4] = {
        float3(1.0f, 0.2f, 0.2f),
        float3(1.0f, 0.8f, 0.2f),
        float3(0.2f, 1.0f, 0.4f),
        float3(0.2f, 0.6f, 1.0f)
    };
    [loop]
    for (uint Index = 0; Index < CascadeCount; ++Index)
    {
        if (ViewDepth < CascadeShadowDistances[Index])
            return float4(Colors[Index], 0.4f);
    }
    discard;
    return 0.0f;
}
