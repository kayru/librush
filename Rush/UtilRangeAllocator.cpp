#include "UtilRangeAllocator.h"
#include "MathCommon.h"
#include "UtilLog.h"

#include <bit>

namespace Rush
{

namespace
{
u64 alignUp(u64 x, u64 alignment) { return (x + alignment - 1) & ~(alignment - 1); }
} // namespace

// Bins are small floats: 3 mantissa bits, exact below 8. Free ranges go in the bin rounded down,
// so every range in bin b is at least as large as any size whose bin rounds up to b.
u32 RangeAllocator::binOf(u64 size, Round round)
{
	if (size < LeafBins)
	{
		return u32(size);
	}
	const u32 shift = u32(std::bit_width(size)) - 4;
	const u32 bin   = ((shift + 1) << 3) | u32((size >> shift) & 7);
	const bool inexact = (size & ((u64(1) << shift) - 1)) != 0;
	return bin + ((round == Round::Up && inexact) ? 1 : 0);
}

void RangeAllocator::reset(u64 size)
{
	m_nodes.clear();
	m_spareNode = Invalid;
	m_size      = size;
	m_freeSize  = size;
	m_topMask   = 0;
	for (u8& mask : m_leafMasks)
	{
		mask = 0;
	}
	for (u32& head : m_binHeads)
	{
		head = Invalid;
	}
	if (size)
	{
		addFree(newNode(0, size));
	}
}

RangeAllocation RangeAllocator::allocate(u64 size, u64 alignment)
{
	RUSH_ASSERT(std::has_single_bit(alignment));
	if (size == 0 || size > m_freeSize || alignment - 1 > ~u64(0) - size)
	{
		return {};
	}
	u32 id = findFit(size, alignment);
	if (id == Invalid)
	{
		return {};
	}
	removeFree(id);
	const u64 offset = alignUp(m_nodes[id].offset, alignment);
	if (offset != m_nodes[id].offset)
	{
		const u32 tail = split(id, offset);
		addFree(id);
		id = tail;
	}
	if (m_nodes[id].size != size)
	{
		addFree(split(id, offset + size));
	}
	m_nodes[id].used = true;
	m_freeSize -= size;
	return {offset, id};
}

void RangeAllocator::free(RangeAllocation allocation)
{
	u32 id = allocation.id;
	RUSH_ASSERT(id < m_nodes.size() && m_nodes[id].used);
	m_nodes[id].used = false;
	m_freeSize += m_nodes[id].size;
	const u32 next = m_nodes[id].next;
	if (next != Invalid && !m_nodes[next].used)
	{
		removeFree(next);
		absorbNext(id);
	}
	const u32 prev = m_nodes[id].prev;
	if (prev != Invalid && !m_nodes[prev].used)
	{
		removeFree(prev);
		absorbNext(prev);
		id = prev;
	}
	addFree(id);
}

u64 RangeAllocator::largestFreeRange() const
{
	if (!m_topMask)
	{
		return 0;
	}
	const u32 top = u32(std::bit_width(m_topMask)) - 1;
	const u32 bin = top * LeafBins + u32(std::bit_width(u32(m_leafMasks[top]))) - 1;
	u64 largest = 0;
	for (u32 id = m_binHeads[bin]; id != Invalid; id = m_nodes[id].binNext)
	{
		largest = max(largest, m_nodes[id].size);
	}
	return largest;
}

u32 RangeAllocator::findBin(u32 minBin) const
{
	const u32 top    = minBin / LeafBins;
	const u32 leaves = m_leafMasks[top] & (0xFFu << (minBin % LeafBins));
	if (leaves)
	{
		return top * LeafBins + u32(std::countr_zero(leaves));
	}
	const u64 tops = top + 1 < TopBins ? m_topMask & (~u64(0) << (top + 1)) : 0;
	if (!tops)
	{
		return Invalid;
	}
	const u32 t = u32(std::countr_zero(tops));
	return t * LeafBins + u32(std::countr_zero(u32(m_leafMasks[t])));
}

u32 RangeAllocator::findFit(u64 size, u64 alignment) const
{
	auto fits = [&](u32 id)
	{
		const Node& n = m_nodes[id];
		return alignUp(n.offset, alignment) - n.offset + size <= n.size;
	};

	// O(1): the smallest bin that guarantees the size; then one that guarantees the padding too
	const u32 bin = findBin(binOf(size, Round::Up));
	if (bin != Invalid && fits(m_binHeads[bin]))
	{
		return m_binHeads[bin];
	}
	if (alignment > 1)
	{
		const u32 paddedBin = findBin(binOf(size + alignment - 1, Round::Up));
		if (paddedBin != Invalid)
		{
			return m_binHeads[paddedBin];
		}
	}

	// Nearly full: search the remaining bins that can hold a fit
	for (u32 b = findBin(binOf(size, Round::Down)); b != Invalid; b = findBin(b + 1))
	{
		for (u32 id = m_binHeads[b]; id != Invalid; id = m_nodes[id].binNext)
		{
			if (fits(id))
			{
				return id;
			}
		}
	}
	return Invalid;
}

u32 RangeAllocator::newNode(u64 offset, u64 size)
{
	u32 id = m_spareNode;
	if (id != Invalid)
	{
		m_spareNode = m_nodes[id].binNext;
	}
	else
	{
		id = u32(m_nodes.size());
		m_nodes.emplace_back();
	}
	m_nodes[id] = Node{.offset = offset, .size = size};
	return id;
}

// Splits node id at offset at; the new node takes the tail and is returned
u32 RangeAllocator::split(u32 id, u64 at)
{
	const Node n    = m_nodes[id];
	const u32  tail = newNode(at, n.offset + n.size - at);
	m_nodes[tail].prev = id;
	m_nodes[tail].next = n.next;
	if (n.next != Invalid)
	{
		m_nodes[n.next].prev = tail;
	}
	m_nodes[id].next = tail;
	m_nodes[id].size = at - n.offset;
	return tail;
}

void RangeAllocator::absorbNext(u32 id)
{
	const u32  next = m_nodes[id].next;
	const Node n    = m_nodes[next];
	m_nodes[id].size += n.size;
	m_nodes[id].next = n.next;
	if (n.next != Invalid)
	{
		m_nodes[n.next].prev = id;
	}
	m_nodes[next].binNext = m_spareNode;
	m_spareNode           = next;
}

void RangeAllocator::addFree(u32 id)
{
	const u32 bin = binOf(m_nodes[id].size, Round::Down);
	Node&     n   = m_nodes[id];
	n.binPrev     = Invalid;
	n.binNext     = m_binHeads[bin];
	if (n.binNext != Invalid)
	{
		m_nodes[n.binNext].binPrev = id;
	}
	m_binHeads[bin] = id;
	m_leafMasks[bin / LeafBins] |= u8(1u << (bin % LeafBins));
	m_topMask |= u64(1) << (bin / LeafBins);
}

void RangeAllocator::removeFree(u32 id)
{
	const Node& n = m_nodes[id];
	if (n.binNext != Invalid)
	{
		m_nodes[n.binNext].binPrev = n.binPrev;
	}
	if (n.binPrev != Invalid)
	{
		m_nodes[n.binPrev].binNext = n.binNext;
		return;
	}
	const u32 bin   = binOf(n.size, Round::Down);
	m_binHeads[bin] = n.binNext;
	if (n.binNext == Invalid)
	{
		const u32 top = bin / LeafBins;
		m_leafMasks[top] &= u8(~(1u << (bin % LeafBins)));
		if (!m_leafMasks[top])
		{
			m_topMask &= ~(u64(1) << top);
		}
	}
}

} // namespace Rush
