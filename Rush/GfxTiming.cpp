#include "GfxTiming.h"

#include <algorithm>
#include <cstring>

namespace Rush
{

template <typename T> static void popFront(DynamicArray<T>& arr)
{
	RUSH_ASSERT(!arr.empty());
	for (size_t i = 1; i < arr.size(); ++i)
	{
		arr[i - 1] = std::move(arr[i]);
	}
	arr.pop_back();
}

void GfxTimingFrame::reset()
{
	frame         = 0;
	level         = GfxTimingLevel::Frame;
	status        = GfxTimingStatus::None;
	thermalState  = -1.0f;
	droppedFrames = 0;
	scopes.clear();
	for (DynamicArray<u32>& it : openScopes)
	{
		it.clear();
	}
	boundaryQueues.clear();
	boundaryNs.clear();
	for (DynamicArray<GfxTimingInterval>& it : intervals)
	{
		it.clear();
	}
	for (DynamicArray<GfxCpuInterval>& it : busyIntervals)
	{
		it.clear();
	}
	output.clear();
	for (GfxQueueTime& it : queueTimes)
	{
		it = GfxQueueTime();
	}
	for (NameBlock& block : m_nameBlocks)
	{
		block.used = 0;
	}
}

const char* GfxTimingFrame::storeName(const char* name)
{
	if (!name)
	{
		return "";
	}

	const size_t size = std::strlen(name) + 1;

	NameBlock* target = nullptr;
	for (NameBlock& block : m_nameBlocks)
	{
		if (block.size - block.used >= size)
		{
			target = &block;
			break;
		}
	}
	if (!target)
	{
		NameBlock block;
		block.size = std::max<size_t>(4096, size);
		block.data = std::unique_ptr<char[]>(new char[block.size]);
		m_nameBlocks.push_back(std::move(block));
		target = &m_nameBlocks.back();
	}

	char* result = target->data.get() + target->used;
	std::memcpy(result, name, size);
	target->used += size;
	return result;
}

GfxTimingCollector::~GfxTimingCollector() = default;

GfxTimingFrame* GfxTimingCollector::allocateFrame()
{
	if (!m_free.empty())
	{
		GfxTimingFrame* result = m_free.back();
		m_free.pop_back();
		return result;
	}

	m_allFrames.push_back(std::unique_ptr<GfxTimingFrame>(m_createFrame()));
	return m_allFrames.back().get();
}

void GfxTimingCollector::recycle(GfxTimingFrame* frame)
{
	frame->reset();
	m_free.push_back(frame);
}

GfxTimingFrame* GfxTimingCollector::beginFrame(u64 frameIndex)
{
	RUSH_ASSERT_MSG(!m_current, "Timing frame already open");

	GfxTimingFrame* frame = allocateFrame();
	frame->reset();
	frame->frame  = frameIndex;
	frame->level  = m_nextLevel;
	frame->status = m_timestamps ? GfxTimingStatus::None : GfxTimingStatus::Unsupported;

	m_current = frame;
	return frame;
}

void GfxTimingCollector::endFrame()
{
	RUSH_ASSERT(m_current);
	for (const DynamicArray<u32>& it : m_current->openScopes)
	{
		RUSH_ASSERT_MSG(it.empty(), "Open scopes must be closed by the backend before the frame ends");
	}
	m_pending.push_back(m_current);
	m_current = nullptr;
}

void GfxTimingCollector::pushScope(GfxContextType queue, const char* name, u32 beginBoundary)
{
	RUSH_ASSERT(m_current);
	DynamicArray<u32>& open = m_current->openScopes[u32(queue)];

	const u32  parent        = open.empty() ? GfxTimingInvalidIndex : open.back();
	const bool parentDropped = !open.empty() && parent == GfxTimingInvalidIndex;
	if (beginBoundary == GfxTimingInvalidIndex || parentDropped)
	{
		open.push_back(GfxTimingInvalidIndex);
		return;
	}

	GfxTimingScopeRecord record;
	record.name          = m_current->storeName(name);
	record.parent        = parent;
	record.queue         = queue;
	record.beginBoundary = beginBoundary;

	open.push_back(u32(m_current->scopes.size()));
	m_current->scopes.push_back(record);
}

void GfxTimingCollector::popScope(GfxContextType queue, u32 endBoundary)
{
	RUSH_ASSERT(m_current);
	DynamicArray<u32>& open = m_current->openScopes[u32(queue)];
	RUSH_ASSERT_MSG(!open.empty(), "Gfx_EndScope without a matching Gfx_BeginScope");
	if (open.empty())
	{
		return;
	}

	const u32 index = open.back();
	open.pop_back();
	if (index != GfxTimingInvalidIndex)
	{
		m_current->scopes[index].endBoundary = endBoundary;
	}
}

const char* GfxTimingCollector::innermostScopeName(GfxContextType queue) const
{
	if (!m_current)
	{
		return nullptr;
	}
	const DynamicArray<u32>& open = m_current->openScopes[u32(queue)];
	for (size_t i = open.size(); i > 0; --i)
	{
		const u32 index = open[i - 1];
		if (index != GfxTimingInvalidIndex)
		{
			return m_current->scopes[index].name;
		}
	}
	return nullptr;
}

static GfxQueueTime computeQueueTime(DynamicArray<GfxTimingInterval>& intervals, DynamicArray<GfxCpuInterval>& outBusy)
{
	GfxQueueTime result;
	outBusy.clear();
	if (intervals.empty())
	{
		return result;
	}

	std::sort(intervals.begin(), intervals.end(),
	    [](const GfxTimingInterval& a, const GfxTimingInterval& b) { return a.beginNs < b.beginNs; });

	result.beginNs = intervals[0].beginNs;
	u64 runBegin   = intervals[0].beginNs;
	u64 runEnd     = intervals[0].endNs;
	for (size_t i = 1; i < intervals.size(); ++i)
	{
		const GfxTimingInterval& it = intervals[i];
		if (it.beginNs > runEnd)
		{
			result.busyNs += runEnd - runBegin;
			outBusy.push_back({runBegin, runEnd});
			runBegin = it.beginNs;
			runEnd   = it.endNs;
		}
		else
		{
			runEnd = std::max(runEnd, it.endNs);
		}
	}
	result.busyNs += runEnd - runBegin;
	outBusy.push_back({runBegin, runEnd});
	result.endNs = runEnd;
	result.busyIntervals = ArrayView<const GfxCpuInterval>(outBusy.data(), outBusy.size());
	return result;
}

void GfxTimingCollector::buildOutput(GfxTimingFrame& frame)
{
	for (u32 i = 0; i < u32(GfxContextType::count); ++i)
	{
		frame.queueTimes[i] = computeQueueTime(frame.intervals[i], frame.busyIntervals[i]);
	}

	// Chained completion points per queue: missing ones take the previous value, decreasing ones are invalid
	DynamicArray<u64>& values = frame.boundaryNs;
	const u32 boundaryCount = frame.boundaryCount();
	RUSH_ASSERT(values.size() == boundaryCount);

	bool anyValid[u32(GfxContextType::count)] = {};
	bool anyMissing = false;
	for (u32 queue = 0; queue < u32(GfxContextType::count); ++queue)
	{
		u64 last = GfxTimingInvalidTime;
		for (u32 i = 0; i < boundaryCount; ++i)
		{
			if (u32(frame.boundaryQueues[i]) != queue)
			{
				continue;
			}
			u64& value = values[i];
			if (value == GfxTimingInvalidTime)
			{
				anyMissing = true;
				value      = last;
				continue;
			}
			if (anyValid[queue] && value < last)
			{
				frame.status |= GfxTimingStatus::Invalid;
				value = last;
			}
			last            = value;
			anyValid[queue] = true;
		}

		// Leading missing boundaries take the first valid value
		u64 next = GfxTimingInvalidTime;
		for (u32 i = boundaryCount; i > 0; --i)
		{
			if (u32(frame.boundaryQueues[i - 1]) != queue)
			{
				continue;
			}
			if (values[i - 1] == GfxTimingInvalidTime)
			{
				values[i - 1] = next;
			}
			else
			{
				next = values[i - 1];
			}
		}
	}
	if (anyMissing && !(frame.status & GfxTimingStatus::Overflow))
	{
		frame.status |= GfxTimingStatus::Invalid;
	}

	frame.output.clear();
	frame.output.reserve(frame.scopes.size());
	for (const GfxTimingScopeRecord& record : frame.scopes)
	{
		GfxScopeTime scope;
		scope.name   = record.name;
		scope.parent = record.parent;
		scope.queue  = record.queue;
		if (anyValid[u32(record.queue)])
		{
			RUSH_ASSERT(frame.boundaryQueues[record.beginBoundary] == record.queue);
			scope.beginNs = values[record.beginBoundary];
			scope.endNs   = record.endBoundary != GfxTimingInvalidIndex ? values[record.endBoundary] : scope.beginNs;
		}
		else
		{
			frame.status |= GfxTimingStatus::Invalid;
		}
		frame.output.push_back(scope);
	}
}

void GfxTimingCollector::completeOldest()
{
	RUSH_ASSERT(!m_pending.empty());

	GfxTimingFrame* frame = m_pending[0];
	popFront(m_pending);

	buildOutput(*frame);

	m_completed.push_back(frame);
	if (m_completed.size() > MaxCompletedFrames)
	{
		GfxTimingFrame* dropped = m_completed[0];
		popFront(m_completed);
		m_completed[0]->droppedFrames += dropped->droppedFrames + 1;
		recycle(dropped);
	}
}

bool GfxTimingCollector::getFrameTimes(GfxFrameTimes& out)
{
	if (m_returned)
	{
		recycle(m_returned);
		m_returned = nullptr;
	}

	if (m_completed.empty())
	{
		return false;
	}

	GfxTimingFrame* frame = m_completed[0];
	popFront(m_completed);
	m_returned = frame;

	out               = GfxFrameTimes();
	out.frame         = frame->frame;
	out.droppedFrames = frame->droppedFrames;
	out.status        = frame->status;
	out.level         = frame->level;
	out.thermalState  = frame->thermalState;
	out.graphics      = frame->queueTimes[u32(GfxContextType::Graphics)];
	out.compute       = frame->queueTimes[u32(GfxContextType::Compute)];
	out.transfer      = frame->queueTimes[u32(GfxContextType::Transfer)];
	out.scopes        = ArrayView<const GfxScopeTime>(frame->output.data(), frame->output.size());
	return true;
}

}
