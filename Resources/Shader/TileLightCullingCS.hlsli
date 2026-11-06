#pragma pack_matrix(row_major)

#define LIGHT_GRID_CONSTANT_REGISTER b0
#include "LightGridCommon.hlsli"
#include "LightData.hlsli"

#define LIGHT_TILE_SIZE LIGHT_GRID_TILE_SIZE
#define INVALID_LIGHT_LIST 0xffffffffu

Texture2D<float> SceneDepth : register(t0);
StructuredBuffer<FLightRenderParameters> SceneLights : register(t1);

// x/y: Point 시작·개수, z/w: Spot 시작·개수.
RWStructuredBuffer<uint4> TileLightRanges : register(u0);
RWStructuredBuffer<uint> TilePointIndices : register(u1);
RWStructuredBuffer<uint> TileSpotIndices : register(u2);

// 0번 원소는 Point, 1번 원소는 Spot의 다음 예약 위치다.
RWStructuredBuffer<uint> TileListAllocator : register(u3);

// 그룹이 공유하는 변수들
groupshared uint MinDepthBits;
groupshared uint MaxDepthBits;
groupshared float3 CellBoundsMin;
groupshared float3 CellBoundsMax;
groupshared uint VisiblePointCount;
groupshared uint VisibleSpotCount;
groupshared uint PointListOffset;
groupshared uint SpotListOffset;
groupshared uint PointWriteCount;
groupshared uint SpotWriteCount;

void InitializeCellState(uint ThreadIndex)
{
    // 그룹 전체가 초기화 완료를 기다린다.
    if (ThreadIndex == 0)
    {
        MinDepthBits = asuint(1.0f);
        MaxDepthBits = asuint(0.0f);
        VisiblePointCount = 0;
        VisibleSpotCount = 0;
        PointWriteCount = 0;
        SpotWriteCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();
}

void BuildCellBounds(uint2 Tile, float MinDepth, float MaxDepth)
{
    float2 PixelMin = float2(Tile * LIGHT_TILE_SIZE);
    float2 PixelMax = min(PixelMin + LIGHT_TILE_SIZE, float2(GridViewWidth, GridViewHeight));
    CellBoundsMin = ReconstructGridViewCorner(PixelMin, MinDepth);
    CellBoundsMax = CellBoundsMin;

    // 앞·뒤 깊이의 네 모서리를 모두 포함하는 뷰 공간 AABB를 만든다.
    [unroll]
    for (uint CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        float2 Corner = float2((CornerIndex & 1u) != 0 ? PixelMax.x : PixelMin.x,
            (CornerIndex & 2u) != 0 ? PixelMax.y : PixelMin.y);
        float Depth = (CornerIndex & 4u) != 0 ? MaxDepth : MinDepth;
        float3 Position = ReconstructGridViewCorner(Corner, Depth);
        CellBoundsMin = min(CellBoundsMin, Position);
        CellBoundsMax = max(CellBoundsMax, Position);
    }
    CellBoundsMin -= 1.0e-4f;
    CellBoundsMax += 1.0e-4f;
}

bool IntersectsCell(float4 PositionRadius)
{
    // 강체 View 변환은 광원의 반경을 바꾸지 않는다.
    float3 Center = mul(float4(PositionRadius.xyz, 1.0f), GridView).xyz;
    float3 Delta = Center - clamp(Center, CellBoundsMin, CellBoundsMax);
    return dot(Delta, Delta) <= PositionRadius.w * PositionRadius.w;
}

bool IntersectsPointCell(FLightRenderParameters Light)
{
    return IntersectsCell(float4(Light.Position, Light.Radius));
}

bool IntersectsSpotCell(FLightRenderParameters Light)
{
    float Radius = max(Light.Radius, 0.001f);

    // 현재 구현처럼 거리 구와 이동한 포함 구를 모두 검사한다.
    if (!IntersectsCell(float4(Light.Position, Radius)))
        return false;

    float CosOuter = saturate(Light.OuterConeCos);
    float Offset;
    float SphereRadius;
    if (CosOuter >= 0.7071067812f)
    {
        // 반각 45도 이하: 시작점과 바깥 테두리에 접하는 포함 구다.
        Offset = Radius / (2.0f * CosOuter);
        SphereRadius = Offset;
    }
    else
    {
        // 넓은 Spot: 바깥 테두리의 중심을 포함 구의 중심으로 사용한다.
        Offset = Radius * CosOuter;
        SphereRadius = Radius * sqrt(saturate(1.0f - CosOuter * CosOuter));
    }

    float3 Direction = normalize(Light.Direction);
    float3 Center = Light.Position + Direction * Offset;
    return IntersectsCell(float4(Center, SphereRadius));
}

uint ReserveCellList(uint Kind, uint Count, uint Capacity)
{
    // 빈 목록에는 공간을 예약하지 않는다.
    if (Count == 0)
        return 0;

    uint Start;
    InterlockedAdd(TileListAllocator[Kind], 0, Start);

    // 기존 비교 후 교환 방식으로 서로 겹치지 않는 구간을 예약한다.
    [allow_uav_condition]
    while (true)
    {
        if (Count > Capacity - Start)
            return INVALID_LIGHT_LIST;

        uint Previous;
        InterlockedCompareExchange(TileListAllocator[Kind], Start, Start + Count, Previous);
        if (Previous == Start)
            return Start;
        Start = Previous;
    }
}

void CullCellLights(uint CellIndex, uint ThreadIndex, uint ThreadCount)
{
    // 첫 검사에서는 후보 개수만 센다. 실제 그룹 크기로 광원 검사를 분담한다.
    for (uint PointIndex = ThreadIndex; PointIndex < GridPointCount; PointIndex += ThreadCount)
        if (IntersectsPointCell(SceneLights[GridLightOffsets.x + PointIndex]))
            InterlockedAdd(VisiblePointCount, 1);

    for (uint SpotIndex = ThreadIndex; SpotIndex < GridSpotCount; SpotIndex += ThreadCount)
        if (IntersectsSpotCell(SceneLights[GridLightOffsets.y + SpotIndex]))
            InterlockedAdd(VisibleSpotCount, 1);
    GroupMemoryBarrierWithGroupSync();

    if (ThreadIndex == 0)
    {
        PointListOffset = ReserveCellList(0, VisiblePointCount, GridPointIndexCapacity);
        SpotListOffset = ReserveCellList(1, VisibleSpotCount, GridSpotIndexCapacity);

        // 예약 실패 시에도 후보 개수를 남겨 fallback과 디버그에 사용한다.
        TileLightRanges[CellIndex] =
            uint4(PointListOffset, VisiblePointCount, SpotListOffset, VisibleSpotCount);
    }
    GroupMemoryBarrierWithGroupSync();

    // 두 번째 검사에서는 확보한 구간에 종류별 원본 배열 인덱스를 저장한다.
    if (PointListOffset != INVALID_LIGHT_LIST)
    {
        for (uint PointIndex = ThreadIndex; PointIndex < GridPointCount; PointIndex += ThreadCount)
        {
            if (IntersectsPointCell(SceneLights[GridLightOffsets.x + PointIndex]))
            {
                uint Slot;
                InterlockedAdd(PointWriteCount, 1, Slot);
                TilePointIndices[PointListOffset + Slot] = PointIndex;
            }
        }
    }

    if (SpotListOffset != INVALID_LIGHT_LIST)
    {
        for (uint SpotIndex = ThreadIndex; SpotIndex < GridSpotCount; SpotIndex += ThreadCount)
        {
            if (IntersectsSpotCell(SceneLights[GridLightOffsets.y + SpotIndex]))
            {
                uint Slot;
                InterlockedAdd(SpotWriteCount, 1, Slot);
                TileSpotIndices[SpotListOffset + Slot] = SpotIndex;
            }
        }
    }
}


[numthreads(LIGHT_TILE_SIZE, LIGHT_TILE_SIZE, 1)]
void mainCS(uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID, uint ThreadIndex : SV_GroupIndex)
{ 
    // Tiled: 그룹 하나가 XY 타일 하나의 깊이를 수집한다.
    uint CellIndex = GetLightGridCellIndexFromSlice(GroupID.xy, 0u);
    InitializeCellState(ThreadIndex);

    uint2 Pixel = GroupID.xy * LIGHT_TILE_SIZE + GroupThreadID.xy;
    if (Pixel.x < GridViewWidth && Pixel.y < GridViewHeight)
    {
        float DeviceZ = SceneDepth.Load(int3(Pixel, 0));
        if (DeviceZ < 1.0f)
        {
            InterlockedMin(MinDepthBits, asuint(DeviceZ));
            InterlockedMax(MaxDepthBits, asuint(DeviceZ));
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // 공유된 값으로 판단하므로 그룹 전체가 함께 반환한다.
    if (MinDepthBits == asuint(1.0f))
    {
        if (ThreadIndex == 0)
            TileLightRanges[CellIndex] = uint4(0, 0, 0, 0);
        return;
    }

    if (ThreadIndex == 0)
    {
        // 기존 D24 깊이의 양자화 오차까지 타일 경계에 포함한다.
        const float DepthQuantum = 1.0f / 16777215.0f;
        float MinDepth = GridDeviceZToViewDepth(max(asfloat(MinDepthBits) - DepthQuantum, 0.0f));
        float MaxDepth = GridDeviceZToViewDepth(min(asfloat(MaxDepthBits) + DepthQuantum, 1.0f));
        BuildCellBounds(GroupID.xy, MinDepth, MaxDepth);
    }
    GroupMemoryBarrierWithGroupSync();

    CullCellLights(CellIndex, ThreadIndex, LIGHT_TILE_SIZE * LIGHT_TILE_SIZE);
}

[numthreads(64, 1, 1)]
void mainClusterCS(uint3 GroupID : SV_GroupID, uint ThreadIndex : SV_GroupIndex)
{
    // Clustered: XY 타일과 Z 구간의 조합 하나에 그룹 하나를 배정한다.
    uint CellIndex = GetLightGridCellIndexFromSlice(GroupID.xy, GroupID.z);
    InitializeCellState(ThreadIndex);

    if (ThreadIndex == 0)
    {
        float MinDepth = GetLightGridSliceBoundaryDepth(GroupID.z);
        float MaxDepth = GetLightGridSliceBoundaryDepth(GroupID.z + 1u);

        // 표면 깊이와 D24 샘플의 경계 오차를 포함하도록 셀을 확장한다.
        const float DepthQuantum = 1.0f / 16777215.0f;
        float MinDeviceZ = GridViewDepthToDeviceZ(MinDepth);
        float MaxDeviceZ = GridViewDepthToDeviceZ(MaxDepth);

        // DeviceZ 왕복 오차로 원래 깊이 범위보다 좁아지지 않게 한다.
        MinDepth = min(MinDepth, GridDeviceZToViewDepth(max(MinDeviceZ - DepthQuantum, 0.0f)));
        MaxDepth = max(MaxDepth, GridDeviceZToViewDepth(min(MaxDeviceZ + DepthQuantum, 1.0f)));
        BuildCellBounds(GroupID.xy, MinDepth, MaxDepth);
    }
    GroupMemoryBarrierWithGroupSync();

    // 고정 깊이 구간을 사용하므로 이 진입점에서는 SceneDepth를 읽지 않는다.
    CullCellLights(CellIndex, ThreadIndex, 64u);
}
