#include "EnginePCH.h"
#include "QuadTree.h"

FQuadTree::FQuadTree(uint32 InRootsize) : RootSize(InRootsize)
{
	const bool bValidRoot = RootSize > 0 && (RootSize & (RootSize - 1)) == 0;
	assert(bValidRoot);

	if (!bValidRoot)
	{
		RootSize = 0;
		return;
	}

	FNode Root{};
	Root.Size = RootSize;
	Nodes.Add(Root);
}

bool FQuadTree::InsertSquare(uint16 SquareSize, FRect& OutRect)
{
	OutRect = {};

	if (!IsValidSquareSize(SquareSize) || Nodes.IsEmpty())
	{
		return false;
	}
	return InsertNode(0, SquareSize, OutRect);
}

bool FQuadTree::IsValidSquareSize(uint32 SquareSize) const
{
	return RootSize > 0 && SquareSize > 0 && SquareSize <= RootSize && (SquareSize & (SquareSize - 1)) == 0;
}

void FQuadTree::SplitNode(int32 NodeIndex)
{
	if (Nodes[NodeIndex].Children[0] != InvalidNodeIndex)
	{
		return;
	}
	const uint32 NodeX = Nodes[NodeIndex].X;
	const uint32 NodeY = Nodes[NodeIndex].Y;
	const uint32 HalfSize = Nodes[NodeIndex].Size / 2;

	if (HalfSize == 0)
	{
		return;
	}
	
	const int32 FirstChildIndex = static_cast<int32>(Nodes.Num());

	for (int32 i = 0; i < 4; ++i)
	{
		FNode Child{};
		Child.X = NodeX + ((i & 1) ? HalfSize : 0u);
		Child.Y = NodeY + ((i & 2) ? HalfSize : 0u);
		Child.Size = HalfSize;
		Nodes.Add(Child);
		Nodes[NodeIndex].Children[i] = FirstChildIndex + i;
	}
}

bool FQuadTree::InsertNode(int32 NodeIndex, uint32 SquareSize, FRect& OutRect)
{
	if (Nodes[NodeIndex].bOccupied || SquareSize > Nodes[NodeIndex].Size)
	{
		return false;
	}

	if (Nodes[NodeIndex].Children[0] != InvalidNodeIndex)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 ChildIndex = Nodes[NodeIndex].Children[i];

			if (InsertNode(ChildIndex, SquareSize, OutRect))
			{
				return true;
			}
		}
		return false;
	}

	if (SquareSize == Nodes[NodeIndex].Size)
	{
		Nodes[NodeIndex].bOccupied = true;
		OutRect.X = static_cast<float>(Nodes[NodeIndex].X);
		OutRect.Y = static_cast<float>(Nodes[NodeIndex].Y);
		OutRect.Width = static_cast<float>(SquareSize);
		OutRect.Height = static_cast<float>(SquareSize);

		return true;
	}
	
	if (Nodes[NodeIndex].Size / 2 < SquareSize)
	{
		return false;
	}

	SplitNode(NodeIndex);

	for (int32 i = 0; i < 4; ++i)
	{
		const int32 ChildIndex = Nodes[NodeIndex].Children[i];
		if (InsertNode(ChildIndex, SquareSize, OutRect))
		{
			return true;
		}
	}

	return false;
}
