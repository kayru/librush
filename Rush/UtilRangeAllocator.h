#pragma once

#include "Rush.h"

#include <vector>

namespace Rush
{

struct RangeAllocation
{
	static constexpr u32 InvalidId = ~0u;

	u64 offset = 0;
	u32 id     = InvalidId;

	bool valid() const { return id != InvalidId; }
};

// Sub-allocates ranges of a linear space (GPU memory blocks, pack files) without touching it.
// TLSF (Masmano et al., 2004) over offsets, after Sebastian Aaltonen's OffsetAllocator
// (github.com/sebbbi/OffsetAllocator): free ranges sit in 8 bins per power of two, found
// with two bit scans; freed ranges merge with free neighbors at once.
// Allocation succeeds whenever some free range fits. Not thread-safe.
class RangeAllocator
{
public:
	explicit RangeAllocator(u64 size = 0) { reset(size); }

	void reset(u64 size);

	// alignment must be a power of two
	RangeAllocation allocate(u64 size, u64 alignment = 1);
	void            free(RangeAllocation allocation);

	u64 allocationSize(RangeAllocation allocation) const { return m_nodes[allocation.id].size; }
	u64 size() const { return m_size; }
	u64 freeSize() const { return m_freeSize; }
	u64 largestFreeRange() const;

private:
	static constexpr u32 Invalid  = ~0u;
	static constexpr u32 TopBins  = 64;
	static constexpr u32 LeafBins = 8;

	enum class Round
	{
		Down,
		Up
	};

	struct Node
	{
		u64  offset  = 0;
		u64  size    = 0;
		u32  binPrev = Invalid;
		u32  binNext = Invalid; // also links spare nodes
		u32  prev    = Invalid; // neighbors in offset order
		u32  next    = Invalid;
		bool used    = false;
	};

	static u32 binOf(u64 size, Round round);

	u32  findBin(u32 minBin) const;
	u32  findFit(u64 size, u64 alignment) const;
	u32  newNode(u64 offset, u64 size);
	u32  split(u32 id, u64 at);
	void absorbNext(u32 id);
	void addFree(u32 id);
	void removeFree(u32 id);

	std::vector<Node> m_nodes;
	u32               m_spareNode = Invalid;
	u64               m_size      = 0;
	u64               m_freeSize  = 0;
	u64               m_topMask   = 0;
	u8                m_leafMasks[TopBins] = {};
	u32               m_binHeads[TopBins * LeafBins];
};

} // namespace Rush
