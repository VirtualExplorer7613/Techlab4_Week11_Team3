// 그림자 캐시: 다시 그릴 아틀라스 타일 하나만 깊이 1(가장 먼 값)로 지운다.
// ClearDepthStencilView는 영역을 지정할 수 없으므로, 뷰포트를 타일에 맞춘 뒤 화면 전체 삼각형을 깊이 1로 그린다.

struct FPSInput
{
    float4 Position : SV_POSITION;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 1.0f, 1.0f);
    return Output;
}

// 색 출력이 없다. 깊이만 쓴다.
void mainPS(FPSInput Input)
{
}
