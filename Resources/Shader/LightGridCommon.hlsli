#ifndef LIGHT_GRID_COMMON_INCLUDED
#define LIGHT_GRID_COMMON_INCLUDED

#pragma pack_matrix(row_major)

// PS는 b3, CS는 포함하기 전에 이 값을 b0로 지정한다.
#ifndef LIGHT_GRID_CONSTANT_REGISTER
#define LIGHT_GRID_CONSTANT_REGISTER b3
#endif

#define LIGHT_GRID_TILE_SIZE 32
#define LIGHT_GRID_MODE_OFF 0u
#define LIGHT_GRID_MODE_TILED 1u
#define LIGHT_GRID_MODE_CLUSTERED 2u

// CPU의 FGridConstants와 동일한 160바이트 배치다.
cbuffer LightGridConstants : register(LIGHT_GRID_CONSTANT_REGISTER)
{
    float4x4 GridView;
    float4 GridProjectionParams; // xy: 역 투영 스케일, zw: 깊이 복원 A/B

    uint GridViewWidth;
    uint GridViewHeight;
    uint GridTileCountX;
    uint GridTileCountY;

    uint GridMode;
    uint GridSliceCount;
    uint GridIsPerspective;
    uint GridPadding;

    uint GridPointCount;
    uint GridSpotCount;
    uint GridPointIndexCapacity;
    uint GridSpotIndexCapacity;

    uint4 GridLightOffsets;
    float4 GridDepthParams; // Near, Far, SliceScale, SliceBias
};

float GridDeviceZToViewDepth(float DeviceZ)
{
    // 이 프로젝트의 양의 선형 뷰 깊이는 +X다.
    return GridIsPerspective != 0 ? GridProjectionParams.w / (DeviceZ - GridProjectionParams.z) 
    : (DeviceZ - GridProjectionParams.w) / GridProjectionParams.z;
}

float GridViewDepthToDeviceZ(float ViewDepth)
{
    // 셀 경계를 깊이 버퍼의 양자화 오차만큼 확장할 때 사용한다.
    return GridIsPerspective != 0 ? GridProjectionParams.z + GridProjectionParams.w / ViewDepth 
    : GridProjectionParams.z * ViewDepth + GridProjectionParams.w;
}

uint GetLightGridSliceIndex(float ViewDepth)
{
    // 원근은 로그 간격, 직교는 선형 간격으로 깊이 구간을 선택한다.
    float Depth = clamp(ViewDepth, GridDepthParams.x, GridDepthParams.y);
    float Slice = GridIsPerspective != 0 ? log2(Depth) * GridDepthParams.z + GridDepthParams.w 
    : Depth * GridDepthParams.z + GridDepthParams.w;

    return uint(clamp(floor(Slice), 0.0f, float(GridSliceCount - 1u)));
}

float GetLightGridSliceBoundaryDepth(uint BoundaryIndex)
{
    // 깊이 구간 선택식의 역함수로 셀의 앞·뒤 경계를 구한다.
    float Coordinate = (float(BoundaryIndex) - GridDepthParams.w) / GridDepthParams.z;
    float Depth = GridIsPerspective != 0 ? exp2(Coordinate) : Coordinate;
    return clamp(Depth, GridDepthParams.x, GridDepthParams.y);
}

uint GetLightGridCellIndexFromSlice(uint2 Tile, uint Slice)
{
    // XY 타일 번호에 깊이 구간 번호를 결합한다.
    return (Slice * GridTileCountY + Tile.y) * GridTileCountX + Tile.x;
}

uint GetLightGridCellIndex(uint2 Pixel, float DeviceZ)
{
    // Tiled는 깊이 구간 0 하나만 사용한다.
    uint2 Tile = Pixel / LIGHT_GRID_TILE_SIZE;
    uint Slice = GridMode == LIGHT_GRID_MODE_CLUSTERED ? GetLightGridSliceIndex(GridDeviceZToViewDepth(DeviceZ)) : 0u;

    return GetLightGridCellIndexFromSlice(Tile, Slice);
}

float3 ReconstructGridViewCorner(float2 PixelCorner, float ViewDepth)
{
    // 픽셀 중심이 아닌 셀 경계 좌표를 뷰 공간으로 복원한다.
    float2 NDC = PixelCorner / float2(GridViewWidth, GridViewHeight) * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float ProjectionW = GridIsPerspective != 0 ? ViewDepth : 1.0f;
    return float3(ViewDepth, NDC * GridProjectionParams.xy * ProjectionW);
}

#endif