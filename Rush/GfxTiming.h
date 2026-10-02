#pragma once


#include "GfxDevice.h"
#include "UtilArray.h"
#include "UtilTimer.h"

#include <memory>

namespace Rush
{

static constexpr u32 GfxTimingInvalidIndex = ~0u;
static constexpr u64 GfxTimingInvalidTime  = ~0ull;

struct GfxTimingInterval
{
	u64 beginNs = 0;
	u64 endNs   = 0;
};

struct GfxTimingScopeRecord
{
	const char* name          = nullptr;
	u32         parent        = GfxTimingInvalidIndex;
	GfxContextType queue      = GfxContextType::Graphics;
	u32         beginBoundary = GfxTimingInvalidIndex;
	u32         endBoundary   = GfxTimingInvalidIndex;
};

// Lives from Gfx_BeginFrame until its results are delivered. Backends derive from it.
struct GfxTimingFrame
{
	virtual ~GfxTimingFrame() = default;

	virtual void reset();

	const char* storeName(const char* name); // copy that lives until the frame is recycled

	u64             frame        = 0;
	GfxTimingLevel  level        = GfxTimingLevel::Frame;
	GfxTimingStatus status       = GfxTimingStatus::None;
	float           thermalState = -1.0f;
	u32             droppedFrames = 0;

	DynamicArray<GfxTimingScopeRecord> scopes;

	// Per queue: scope indices, GfxTimingInvalidIndex for dropped scopes
	DynamicArray<u32> openScopes[u32(GfxContextType::count)];

	// Chained per queue. Backends fill boundaryNs at resolve, GfxTimingInvalidTime when missing.
	DynamicArray<GfxContextType> boundaryQueues;
	DynamicArray<u64>            boundaryNs;

	// Filled by the backend at resolve, CPU ns
	DynamicArray<GfxTimingInterval> intervals[u32(GfxContextType::count)];
	DynamicArray<GfxCpuInterval>    busyIntervals[u32(GfxContextType::count)];

	DynamicArray<GfxScopeTime> output;
	GfxQueueTime               queueTimes[u32(GfxContextType::count)];

	u32 boundaryCount() const { return u32(boundaryQueues.size()); }
	u32 addBoundary(GfxContextType queue)
	{
		boundaryQueues.push_back(queue);
		return boundaryCount() - 1;
	}

private:
	struct NameBlock
	{
		std::unique_ptr<char[]> data;
		size_t                  size = 0;
		size_t                  used = 0;
	};
	DynamicArray<NameBlock> m_nameBlocks;
};

class GfxTimingCollector
{
public:
	using CreateFrameFn = GfxTimingFrame* (*)();

	explicit GfxTimingCollector(CreateFrameFn createFrame) : m_createFrame(createFrame) {}
	~GfxTimingCollector();

	GfxTimingCollector(const GfxTimingCollector&)            = delete;
	GfxTimingCollector& operator=(const GfxTimingCollector&) = delete;

	void           setLevel(GfxTimingLevel level) { m_nextLevel = level; }
	GfxTimingLevel nextLevel() const { return m_nextLevel; }

	GfxTimingFrame* current() const { return m_current; }
	bool            scopesEnabled() const { return m_current && m_current->level != GfxTimingLevel::Frame && m_timestamps; }

	void setTimestampsSupported(bool supported) { m_timestamps = supported; }

	GfxTimingFrame* beginFrame(u64 frameIndex);
	void            endFrame();

	// A begin boundary of GfxTimingInvalidIndex drops the scope (storage overflow)
	bool isTopLevel(GfxContextType queue) const { return openScopeCount(queue) == 0; }
	void pushScope(GfxContextType queue, const char* name, u32 beginBoundary);
	void popScope(GfxContextType queue, u32 endBoundary);
	u32  openScopeCount(GfxContextType queue) const { return m_current ? u32(m_current->openScopes[u32(queue)].size()) : 0; }
	const char* innermostScopeName(GfxContextType queue) const;

	// isolate() runs before top-level scopes at the Isolated level; boundary() returns a boundary index
	template <typename IsolateFn, typename BoundaryFn>
	void beginScope(GfxContextType queue, const char* name, IsolateFn&& isolate, BoundaryFn&& boundary)
	{
		if (isTopLevel(queue) && m_current->level == GfxTimingLevel::Isolated)
		{
			isolate();
		}
		const bool overflow = !!(m_current->status & GfxTimingStatus::Overflow);
		pushScope(queue, name, overflow ? GfxTimingInvalidIndex : boundary());
	}

	template <typename BoundaryFn> void closeOpenScopes(GfxContextType queue, BoundaryFn&& boundary)
	{
		if (openScopeCount(queue) == 0)
		{
			return;
		}
		RUSH_ASSERT_MSG(false, "Scopes left open at the end of the frame or async compute block");
		m_current->status |= GfxTimingStatus::Invalid;
		while (openScopeCount(queue) != 0)
		{
			popScope(queue, boundary());
		}
	}

	u32             pendingCount() const { return u32(m_pending.size()); }
	GfxTimingFrame* pending(u32 i) const { return m_pending[i]; }

	// After the backend filled boundaryNs and intervals of the oldest pending frame
	void completeOldest();

	bool getFrameTimes(GfxFrameTimes& out);

private:
	GfxTimingFrame* allocateFrame();
	void            recycle(GfxTimingFrame* frame);
	void            buildOutput(GfxTimingFrame& frame);

	static constexpr u32 MaxCompletedFrames = 8;

	CreateFrameFn  m_createFrame = nullptr;
	GfxTimingLevel m_nextLevel   = GfxTimingLevel::Frame;
	bool           m_timestamps  = false;

	GfxTimingFrame*               m_current  = nullptr;
	GfxTimingFrame*               m_returned = nullptr;
	DynamicArray<GfxTimingFrame*> m_pending;
	DynamicArray<GfxTimingFrame*> m_completed;
	DynamicArray<GfxTimingFrame*> m_free;

	DynamicArray<std::unique_ptr<GfxTimingFrame>> m_allFrames;
};

struct GfxTimingBlock
{
	u32 block = 0;
	u32 used  = 0;
};

// Returns count consecutive slots as ordinal * blockSize + local, or GfxTimingInvalidIndex when out of blocks
template <typename CreateFn>
u32 Gfx_AllocateTimingSlots(
    DynamicArray<GfxTimingBlock>& frameBlocks, DynamicArray<u32>& freeBlocks, u32 blockSize, u32 count, CreateFn&& createBlock)
{
	if (frameBlocks.empty() || frameBlocks.back().used + count > blockSize)
	{
		u32 block = GfxTimingInvalidIndex;
		if (!freeBlocks.empty())
		{
			block = freeBlocks.back();
			freeBlocks.pop_back();
		}
		else
		{
			block = createBlock();
		}
		if (block == GfxTimingInvalidIndex)
		{
			return GfxTimingInvalidIndex;
		}
		frameBlocks.push_back({block, 0});
	}

	GfxTimingBlock& last = frameBlocks.back();
	const u32       slot = u32(frameBlocks.size() - 1) * blockSize + last.used;
	last.used += count;
	return slot;
}

inline void Gfx_RecordDisplayWait(GfxStats& stats, u64 beginNs, u64 endNs)
{
	stats.displayWaitNs += endNs - beginNs;
	if (stats.displayWaitCount < GfxStats::MaxDisplayWaits)
	{
		stats.displayWaits[stats.displayWaitCount] = {beginNs, endNs};
	}
	++stats.displayWaitCount;
}

// Signed difference of two timestamps that wrap at validBits
inline s64 Gfx_TimestampDelta(u64 a, u64 b, u32 validBits)
{
	if (validBits >= 64)
	{
		return s64(a - b);
	}
	const u64 mask  = (u64(1) << validBits) - 1;
	const u64 delta = (a - b) & mask;
	const u64 half  = u64(1) << (validBits - 1);
	return delta >= half ? s64(delta) - s64(mask) - 1 : s64(delta);
}

}
