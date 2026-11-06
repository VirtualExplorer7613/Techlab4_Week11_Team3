#pragma once

#include <cstdint>

enum class EShaderType
{
	Vertex,
	Pixel,
	Compute,
};

enum class ERenderingPath
{
    Forward,
    Deferred
};

enum class ELightingModel
{
    Gouraud,
    Lambert,
    Phong,
    Unlit // 조명과 그림자 없이 Base Color만 출력한다
};

enum class ELightCullingMode
{
    Off = 0,
    Tiled = 1,
    Clustered = 2
};

enum class EPcfMode : uint32_t
{
    Off = 0,
    Sample8 = 1,
    Sample16 = 2,
    Sample32 = 3,
    PCSS = 4
};
