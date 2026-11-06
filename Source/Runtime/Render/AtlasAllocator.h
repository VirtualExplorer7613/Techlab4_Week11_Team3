#pragma once

#include "Core/Types.h"
#include "Containers/Array.h"
#include "Math/EngineMath.h"

//struct FAtlasRegion
//{
//	uint32 X = 0, Y = 0, Size = 0;
//	uint32 Handle = -1;
//	bool IsValid() const { return Handle != -1; }
//	bool IsValid() const { return Handle != -1; }
//};
//
//class IAtlasAllocator
//{
//public:
//	virtual ~IAtlasAllocator() = default;
//	virtual FAtlasRegion Allocate(uint32 Size) = 0;
//	virtual void Free(const FAtlasRegion& Region) = 0;
//	virtual void Reset() = 0;
//};
//
//class FQuadTreeAtlasAllocator : public IAtlasAllocator
//{
//public:
//	FQuadTreeAtlasAllocator(uint32 InAtlasSize, uint32 InMinTextureSize)
//		:AtlasSize(InAtlasSize), MinTextureSize(InMinTextureSize)
//	{
//		if (AtlasSize % MinTextureSize != 0) return;
//		uint32 Levels = FMath::Log2((float)AtlasSize / MinTextureSize);
//		uint32 Nodes = (1 << (2 * (Levels)) - 1) / 3;
//	}
//
//	~FQuadTreeAtlasAllocator() = default;
//	virtual FAtlasRegion Allocate(uint32 Size) override;
//	virtual void Free(const FAtlasRegion& Region) override;
//	virtual void Reset() override;
//private:
//	void FindEnableRegion(uint32 CurNode, uint32 CurX, uint32 CurY, uint32 Size, FAtlasRegion& Out);
//
//	enum class ENodeState : uint8 { Free, Split, Used };
//
//	uint32 AtlasSize = 0;
//	uint32 MinTextureSize = 0;
//	TArray<ENodeState> State;
//};