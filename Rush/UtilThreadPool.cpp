#include "UtilThreadPool.h"
#include "UtilLog.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <deque>
#include <new>
#include <thread>

#if defined(__linux__)
#include <linux/futex.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) && __has_include(<os/os_sync_wait_on_address.h>)
#include <os/os_sync_wait_on_address.h>
#define RUSH_OS_SYNC_WAIT 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4324) // intentional cache line padding
#endif

namespace Rush
{

struct TaskDomain
{
	int priority = 0;
	std::deque<ThreadPool::Task> tasks;
};

namespace
{

constexpr u32 kMaxGroups = ThreadPool::Desc::Cpu::kMaxGroups;

struct Topology
{
	DynamicArray<u8> groupOfCpu;
	u32 groupCount = 1;
	u32 coresInGroup[kMaxGroups] = {};
	u8 groupsNearestFirst[kMaxGroups][kMaxGroups] = {};
};

Topology buildTopology(const DynamicArray<ThreadPool::Desc::Cpu>& cpus)
{
	Topology t;
	if (cpus.empty())
	{
		t.coresInGroup[0] = std::max(1u, std::thread::hardware_concurrency());
		return t;
	}
	u32 maxProcessorNumber = 0;
	for (const ThreadPool::Desc::Cpu& c : cpus)
	{
		maxProcessorNumber = std::max(maxProcessorNumber, c.processorNumber);
		t.groupCount = std::max(t.groupCount, std::min<u32>(c.group, kMaxGroups - 1) + 1);
	}
	t.groupOfCpu.resize(size_t(maxProcessorNumber) + 1, u8(0));
	DynamicArray<u64> groupCores;
	for (const ThreadPool::Desc::Cpu& c : cpus)
	{
		const u32 group = std::min<u32>(c.group, kMaxGroups - 1);
		t.groupOfCpu[c.processorNumber] = u8(group);
		groupCores.push_back(u64(group) << 32 | c.core);
	}
	std::sort(groupCores.begin(), groupCores.end());
	for (size_t i = 0; i < groupCores.size(); ++i)
	{
		if (i == 0 || groupCores[i] != groupCores[i - 1])
		{
			++t.coresInGroup[groupCores[i] >> 32];
		}
	}
	// Nearest by number. Groups 0-1, 2-3, ... often share a die, so on ties the paired group comes first
	for (u32 from = 0; from < t.groupCount; ++from)
	{
		const auto distance = [from](u32 to) { return to > from ? to - from : from - to; };
		const auto paired = [from](u32 to) { return to / 2 == from / 2; };
		DynamicArray<u32> order(t.groupCount);
		for (u32 to = 0; to < t.groupCount; ++to)
		{
			order[to] = to;
		}
		std::stable_sort(order.begin(), order.end(), [&](u32 a, u32 b)
		{
			if (distance(a) != distance(b))
			{
				return distance(a) < distance(b);
			}
			return paired(a) && !paired(b);
		});
		for (u32 i = 0; i < t.groupCount; ++i)
		{
			t.groupsNearestFirst[from][i] = u8(order[i]);
		}
	}
	return t;
}

void pauseCpu()
{
#if defined(__x86_64__) || defined(_M_X64)
	_mm_pause();
#elif defined(__aarch64__)
	__asm__ __volatile__("isb");
#endif
}

s64 clockNow()
{
	return s64(std::chrono::steady_clock::now().time_since_epoch().count());
}

template <typename P>
bool spinFor(u32 us, P&& done)
{
	if (us == 0)
	{
		return done();
	}
	const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(us);
	for (u32 i = 0;; ++i)
	{
		if (done())
		{
			return true;
		}
		pauseCpu();
		if ((i & 63) == 63 && std::chrono::steady_clock::now() >= until)
		{
			return done();
		}
	}
}

void waitOnWord(std::atomic<u32>& word, u32 expected)
{
#if defined(__linux__)
	syscall(SYS_futex, reinterpret_cast<u32*>(&word), FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0);
#elif defined(_WIN32)
	WaitOnAddress(&word, &expected, sizeof(expected), INFINITE);
#elif defined(RUSH_OS_SYNC_WAIT)
	if (__builtin_available(macOS 14.4, iOS 17.4, *))
	{
		os_sync_wait_on_address(&word, expected, sizeof(u32), OS_SYNC_WAIT_ON_ADDRESS_NONE);
	}
	else
	{
		word.wait(expected, std::memory_order_acquire);
	}
#else
	word.wait(expected, std::memory_order_acquire);
#endif
}

void wakeOneOnWord(std::atomic<u32>& word)
{
#if defined(__linux__)
	syscall(SYS_futex, reinterpret_cast<u32*>(&word), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
#elif defined(_WIN32)
	WakeByAddressSingle(&word);
#elif defined(RUSH_OS_SYNC_WAIT)
	if (__builtin_available(macOS 14.4, iOS 17.4, *))
	{
		os_sync_wake_by_address_any(&word, sizeof(u32), OS_SYNC_WAKE_BY_ADDRESS_NONE);
	}
	else
	{
		word.notify_one();
	}
#else
	word.notify_one();
#endif
}

thread_local TaskDomain* t_currentDomain = nullptr;
thread_local const ThreadPoolWorkers* t_workersOfThisThread = nullptr;

enum : u32
{
	kAwake = 0,
	kAsleep = 1,
	kRung = 2,
};

constexpr u32 kJoinWaiting = 1u << 31;

struct alignas(64) Lane
{
	std::atomic<u64> unclaimed;
};

constexpr u64 packRange(u64 begin, u64 end)
{
	return (begin << 32) | end;
}

constexpr u64 rangeBegin(u64 v)
{
	return v >> 32;
}

constexpr u64 rangeEnd(u64 v)
{
	return v & 0xffffffffu;
}

constexpr u64 rangeSize(u64 v)
{
	return rangeEnd(v) - rangeBegin(v);
}

u32 pickTarget(u32 laneCount, u32 own)
{
	thread_local u64 state = 0x9E3779B97F4A7C15ull ^ u64(reinterpret_cast<uintptr_t>(&state));
	state ^= state << 13;
	state ^= state >> 7;
	state ^= state << 17;
	const u32 lane = u32(state % (laneCount - 1));
	return lane >= own ? lane + 1 : lane;
}

struct ParallelRun
{
	void (*run)(const void* context, u64 first, u64 last);
	const void* context;
	u64 count;
	Lane* lanes;
	u32 width;
	alignas(64) std::atomic<u32> reservedLaneCount;
	alignas(64) std::atomic<u32> helpersWorking; // NOTE: kJoinWaiting is the top bit
	alignas(64) std::atomic<u32> helpersHoldingRun;

	ArrayView<Lane> reservedLanes() const { return ArrayView<Lane>(lanes, reservedLaneCount.load(std::memory_order_acquire)); }
};

struct alignas(64) WorkerSlot
{
	std::atomic<u32> doorbell = kAwake;
	std::atomic<u32> lane = 0;
	std::atomic<s64> rungAt = 0;
	std::atomic<ParallelRun*> job = nullptr;
};

} // namespace

u32 ThreadPool::currentProcessorNumber()
{
#if defined(__linux__)
	const int cpu = sched_getcpu();
	return cpu >= 0 ? u32(cpu) : ~0u;
#elif defined(_WIN32)
	PROCESSOR_NUMBER number;
	GetCurrentProcessorNumberEx(&number);
	return u32(number.Group) * 64 + number.Number;
#else
	return ~0u;
#endif
}

struct ThreadPoolWorkers
{
	alignas(64) std::mutex mutex;
	alignas(64) std::atomic<u32> queuedTasks = 0;
	std::atomic<u32> runningTasks = 0;
	std::atomic<bool> shutdown = false;
	alignas(64) std::atomic<u64> idleWorkers = 0;
	alignas(64) std::atomic<u8> idleGroup[ThreadPool::kMaxThreads] = {};
	std::atomic<u32> count = 0;
	Topology topology;
	u32 minWorkPerHelper = 0;
	alignas(64) std::atomic<s64> sleeperWakeTicks = 0;
	WorkerSlot slots[ThreadPool::kMaxThreads];

	u8 currentGroup() const;
	u32 claim(ArrayView<u32> out);
	u64 choose(u64 idleNow, u32 want) const;
	void ring(u32 worker, ParallelRun* run, u32 lane);
	u32 recruit(ParallelRun& run, u32 want);
};

u8 ThreadPoolWorkers::currentGroup() const
{
	const u32 cpu = ThreadPool::currentProcessorNumber();
	return cpu < topology.groupOfCpu.size() ? topology.groupOfCpu[cpu] : 0;
}

u64 ThreadPoolWorkers::choose(u64 idleNow, u32 want) const
{
	const Topology& t = topology;
	const u32 groups = t.groupCount;
	const u32 n = count.load(std::memory_order_relaxed);
	u64 groupWorkers[kMaxGroups];
	int freeCores[kMaxGroups];
	for (u32 g = 0; g < groups; ++g)
	{
		groupWorkers[g] = 0;
		freeCores[g] = int(t.coresInGroup[g]);
	}
	for (u32 w = 0; w < n; ++w)
	{
		const u32 g = idleGroup[w].load(std::memory_order_relaxed);
		const u64 bit = u64(1) << w;
		groupWorkers[g] |= bit;
		freeCores[g] -= (idleNow & bit) ? 0 : 1;
	}
	const u32 nearGroup = currentGroup();
	if (t_workersOfThisThread != this)
	{
		--freeCores[nearGroup];
	}
	u64 pick = 0;
	u32 picked = 0;
	for (u32 pass = 0; pass < 2 && picked < want; ++pass)
	{
		for (u32 i = 0; i < groups && picked < want; ++i)
		{
			const u32 g = t.groupsNearestFirst[nearGroup][i];
			int cap = pass == 0 ? freeCores[g] : int(ThreadPool::kMaxThreads);
			for (u64 bits = idleNow & groupWorkers[g] & ~pick; bits != 0 && picked < want && cap > 0; bits &= bits - 1, ++picked, --cap)
			{
				pick |= bits & (~bits + 1);
			}
		}
	}
	return pick;
}

u32 ThreadPoolWorkers::claim(ArrayView<u32> out)
{
	const u32 want = u32(out.size());
	u32 got = 0;
	for (u32 attempt = 0; attempt < 4 && got < want; ++attempt)
	{
		// seq_cst pairs with the worker's idle recheck
		const u64 idleNow = idleWorkers.load(std::memory_order_seq_cst);
		if (idleNow == 0)
		{
			break;
		}
		const u64 pick = choose(idleNow, want - got);
		// Bits we cleared are ours to ring
		for (u64 mine = idleWorkers.fetch_and(~pick, std::memory_order_acq_rel) & pick; mine != 0; mine &= mine - 1)
		{
			out[got++] = u32(std::countr_zero(mine));
		}
	}
	return got;
}

void ThreadPoolWorkers::ring(u32 worker, ParallelRun* run, u32 lane)
{
	WorkerSlot& slot = slots[worker];
	slot.lane.store(lane, std::memory_order_relaxed);
	slot.rungAt.store(clockNow(), std::memory_order_relaxed);
	slot.job.store(run, std::memory_order_relaxed);
	if (slot.doorbell.exchange(kRung, std::memory_order_acq_rel) == kAsleep)
	{
		wakeOneOnWord(slot.doorbell);
	}
}

u32 ThreadPoolWorkers::recruit(ParallelRun& run, u32 want)
{
	const u32 capacity = run.width;
	u32 first = run.reservedLaneCount.load(std::memory_order_relaxed);
	do
	{
		want = std::min(want, capacity - first);
		if (want == 0)
		{
			return 0;
		}
	} while (!run.reservedLaneCount.compare_exchange_weak(first, first + want, std::memory_order_relaxed));
	u32 helpers[ThreadPool::kMaxThreads];
	const u32 got = claim(ArrayView<u32>(helpers, want));
	u32 reserved = first + want;
	run.reservedLaneCount.compare_exchange_strong(reserved, first + got, std::memory_order_relaxed);
	run.helpersWorking.fetch_add(got, std::memory_order_relaxed);
	run.helpersHoldingRun.fetch_add(got, std::memory_order_relaxed);
	for (u32 i = 0; i < got; ++i)
	{
		ring(helpers[i], &run, first + i);
	}
	return got;
}

namespace
{

constexpr u32 kInitialHelpers = 3;
constexpr u32 kMaxRecruitsPerChunk = 3;

bool steal(ArrayView<Lane> lanes, u32 own)
{
	const u32 n = u32(lanes.size());
	if (n < 2)
	{
		return false;
	}
	for (u32 attempt = 0;; ++attempt)
	{
		u32 victim = own;
		u64 seen = 0;
		if (attempt < 2 * n)
		{
			for (u32 pick = 0; pick < 2; ++pick)
			{
				const u32 l = pickTarget(n, own);
				const u64 v = lanes[l].unclaimed.load(std::memory_order_relaxed);
				if (rangeSize(v) > rangeSize(seen))
				{
					victim = l;
					seen = v;
				}
			}
			if (victim == own)
			{
				continue;
			}
		}
		else
		{
			for (u32 l = 0; l < n && victim == own; ++l)
			{
				const u64 v = lanes[l].unclaimed.load(std::memory_order_relaxed);
				if (l != own && rangeSize(v) > 0)
				{
					victim = l;
					seen = v;
				}
			}
			if (victim == own)
			{
				return false;
			}
		}
		const u64 end = rangeEnd(seen);
		const u64 half = (rangeSize(seen) + 1) / 2;
		if (lanes[victim].unclaimed.compare_exchange_strong(seen, packRange(rangeBegin(seen), end - half), std::memory_order_relaxed))
		{
			lanes[own].unclaimed.store(packRange(end - half, end), std::memory_order_relaxed);
			return true;
		}
	}
}

bool recruitAfter(ParallelRun& run, ThreadPoolWorkers& workers, u64 items, s64 ticks)
{
	const u32 participants = run.reservedLaneCount.load(std::memory_order_relaxed);
	if (participants >= run.width || workers.idleWorkers.load(std::memory_order_relaxed) == 0)
	{
		return false;
	}
	u64 left = 0;
	for (const Lane& lane : run.reservedLanes())
	{
		left += rangeSize(lane.unclaimed.load(std::memory_order_relaxed));
	}
	const double timeLeft = double(left) * double(ticks) / double(items);
	const double minShare = double(workers.minWorkPerHelper) * double(std::max<s64>(workers.sleeperWakeTicks.load(std::memory_order_relaxed), 1));
	u32 want = 0;
	while (want < kMaxRecruitsPerChunk && timeLeft > minShare * double(participants + want + 1))
	{
		++want;
	}
	if (want != 0)
	{
		workers.recruit(run, want);
	}
	return true;
}

void workLanes(ParallelRun& run, u32 own, ThreadPoolWorkers& workers) noexcept
{
	void (*const fn)(const void*, u64, u64) = run.run;
	const void* const context = run.context;
	const u64 maxTake = std::max<u64>(1, run.count / (u64(run.width) * 8));
	std::atomic<u64>& mine = run.lanes[own].unclaimed;
	bool measured = false;
	do
	{
		u64 v = mine.load(std::memory_order_relaxed);
		while (rangeSize(v) > 0)
		{
			const u64 first = rangeBegin(v);
			const u64 take = std::min(std::max<u64>(1, rangeSize(v) / 8), maxTake);
			if (mine.compare_exchange_weak(v, packRange(first + take, rangeEnd(v)), std::memory_order_relaxed))
			{
				const bool timed = !measured && run.reservedLaneCount.load(std::memory_order_relaxed) < run.width;
				const s64 start = timed ? clockNow() : 0;
				fn(context, first, first + take);
				if (timed)
				{
					measured = recruitAfter(run, workers, take, clockNow() - start);
				}
				v = mine.load(std::memory_order_relaxed);
			}
		}
	} while (steal(run.reservedLanes(), own));
}

} // namespace

ThreadPool::ThreadPool()
	: ThreadPool(Desc{})
{
}

ThreadPool::ThreadPool(const Desc& desc)
	: m_desc(desc)
	, m_workers(std::make_unique<ThreadPoolWorkers>())
{
	m_workers->topology = buildTopology(desc.cpus);
	m_workers->minWorkPerHelper = desc.minWorkPerHelper ? desc.minWorkPerHelper : 2 * std::max(1u, std::thread::hardware_concurrency());
	m_defaultDomain = createDomain(0);
	const u32 hardware = std::max(1u, std::thread::hardware_concurrency()) - 1;
	startWorkers(std::min(desc.workers == Desc::kHardwareWorkers ? hardware : desc.workers, kMaxThreads));
}

TaskDomain* ThreadPool::createDomain(int priority)
{
	auto domain = std::make_unique<TaskDomain>();
	domain->priority = priority;
	TaskDomain* result = domain.get();
	std::lock_guard<std::mutex> lock(m_workers->mutex);
	const size_t at = size_t(std::find_if(m_domains.begin(), m_domains.end(), [&](const std::unique_ptr<TaskDomain>& p) { return p->priority <= priority; }) - m_domains.begin());
	RUSH_ASSERT_MSG(at == m_domains.size() || m_domains[at]->priority != priority, "Two task domains with priority %d", priority);
	m_domains.push_back(std::move(domain));
	std::rotate(m_domains.begin() + at, m_domains.end() - 1, m_domains.end());
	return result;
}

TaskDomain* ThreadPool::currentDomain()
{
	return t_currentDomain;
}

void ThreadPool::setCurrentDomain(TaskDomain* domain)
{
	t_currentDomain = domain;
}

TaskDomain* ThreadPool::resolveDomain(TaskDomain* domain) const
{
	for (const std::unique_ptr<TaskDomain>& own : m_domains)
	{
		if (own.get() == domain)
		{
			return domain;
		}
	}
	return m_defaultDomain;
}

TaskDomain* ThreadPool::eligibleDomain()
{
	for (const std::unique_ptr<TaskDomain>& domain : m_domains)
	{
		if (!domain->tasks.empty())
		{
			return domain.get();
		}
	}
	return nullptr;
}

u32 ThreadPool::numIdleWorkers() const
{
	return u32(std::popcount(m_workers->idleWorkers.load()));
}

u32 ThreadPool::numWorkerThreads() const
{
	return m_workers->count.load();
}

void ThreadPool::startWorkers(u32 numWorkers)
{
	numWorkers = std::min(numWorkers, kMaxThreads);
	std::unique_lock<std::mutex> lock(m_workers->mutex);
	if (m_threads.size() < numWorkers)
	{
		m_workers->count.store(numWorkers);
	}
	while (m_threads.size() < numWorkers)
	{
		const u32 index = u32(m_threads.size());
		m_threads.push_back(std::thread([this, index]() { workerMain(index); }));
	}
}

void ThreadPool::workerMain(u32 index)
{
	ThreadPoolWorkers& workers = *m_workers;
	WorkerSlot& slot = workers.slots[index];
	t_workersOfThisThread = &workers;
	const u64 bit = u64(1) << index;
	for (;;)
	{
		while (doWorkInternal())
		{
		}
		if (m_workers->shutdown.load(std::memory_order_acquire))
		{
			break;
		}
		if (const u8 group = workers.currentGroup(); workers.idleGroup[index].load(std::memory_order_relaxed) != group)
		{
			workers.idleGroup[index].store(group, std::memory_order_relaxed);
		}
		slot.doorbell.store(kAwake, std::memory_order_relaxed);
		workers.idleWorkers.fetch_or(bit, std::memory_order_seq_cst);

		if (m_workers->queuedTasks.load(std::memory_order_seq_cst) != 0 || m_workers->shutdown.load(std::memory_order_seq_cst))
		{
			if (workers.idleWorkers.fetch_and(~bit, std::memory_order_seq_cst) & bit)
			{
				continue;
			}
			// Someone claimed us, so a ring is on its way
		}

		u32 expected = kAwake;
		const bool slept = slot.doorbell.compare_exchange_strong(expected, kAsleep, std::memory_order_acq_rel);
		while (slot.doorbell.load(std::memory_order_acquire) == kAsleep)
		{
			waitOnWord(slot.doorbell, kAsleep);
		}
		if (ParallelRun* run = slot.job.exchange(nullptr, std::memory_order_acquire))
		{
			if (slept)
			{
				workers.sleeperWakeTicks.store(clockNow() - slot.rungAt.load(std::memory_order_relaxed), std::memory_order_relaxed);
			}
			workLanes(*run, slot.lane.load(std::memory_order_relaxed), workers);
			if (run->helpersWorking.fetch_sub(1, std::memory_order_acq_rel) == (kJoinWaiting | 1))
			{
				wakeOneOnWord(run->helpersWorking);
			}
			run->helpersHoldingRun.fetch_sub(1, std::memory_order_release);
		}
	}
}

ThreadPool::~ThreadPool()
{
	m_workers->shutdown.store(true, std::memory_order_seq_cst);
	for (u64 idle = m_workers->idleWorkers.exchange(0, std::memory_order_seq_cst); idle != 0; idle &= idle - 1)
	{
		m_workers->ring(u32(std::countr_zero(idle)), nullptr, 0);
	}
	for (std::thread& t : m_threads)
	{
		t.join();
	}
}

void ThreadPool::wakeForQueue(u32 count)
{
	u32 claimed[kMaxThreads];
	const u32 n = m_workers->claim(ArrayView<u32>(claimed, std::min(count, kMaxThreads)));
	for (u32 i = 0; i < n; ++i)
	{
		m_workers->ring(claimed[i], nullptr, 0);
	}
}

ThreadPool::Task ThreadPool::popTask()
{
	std::lock_guard<std::mutex> lock(m_workers->mutex);
	Task result;
	if (TaskDomain* domain = eligibleDomain())
	{
		result = std::move(domain->tasks.front());
		domain->tasks.pop_front();
		m_workers->queuedTasks.fetch_sub(1, std::memory_order_relaxed);
	}
	return result;
}

void ThreadPool::pushTask(TaskFunction&& fun, bool allowImmediateExecution)
{
	pushTask(Task{std::move(fun), nullptr, currentDomain()}, allowImmediateExecution);
}

void ThreadPool::pushTask(Task&& task, bool allowImmediateExecution)
{
	std::unique_lock<std::mutex> lock(m_workers->mutex);
	task.domain = resolveDomain(task.domain);
	if (m_threads.empty() || m_workers->shutdown.load(std::memory_order_relaxed) || (m_workers->runningTasks.load() >= m_threads.size() && allowImmediateExecution))
	{
		lock.unlock();
		runTask(task);
		return;
	}
	task.domain->tasks.push_back(std::move(task));
	m_workers->queuedTasks.fetch_add(1, std::memory_order_seq_cst);
	lock.unlock();
	wakeForQueue(1);
}

void ThreadPool::pushTasks(TaskDomain* domain, DynamicArray<Task>&& tasks)
{
	if (tasks.empty())
	{
		return;
	}
	std::unique_lock<std::mutex> lock(m_workers->mutex);
	domain = resolveDomain(domain);
	const bool runHere = m_threads.empty() || m_workers->shutdown.load(std::memory_order_relaxed);
	for (Task& task : tasks)
	{
		task.domain = domain;
		if (!runHere)
		{
			domain->tasks.push_back(std::move(task));
		}
	}
	if (!runHere)
	{
		m_workers->queuedTasks.fetch_add(u32(tasks.size()), std::memory_order_seq_cst);
		lock.unlock();
		wakeForQueue(u32(tasks.size()));
		return;
	}
	lock.unlock();
	for (Task& task : tasks)
	{
		runTask(task);
	}
}

void ThreadPool::runTask(Task& task) noexcept
{
	++m_workers->runningTasks;
	TaskDomain* const previous = t_currentDomain;
	t_currentDomain = task.domain;
	task.function();
	task.function = nullptr;
	t_currentDomain = previous;
	--m_workers->runningTasks;
	if (task.group)
	{
		task.group->taskFinished();
	}
}

bool ThreadPool::doWorkInternal()
{
	if (m_workers->queuedTasks.load(std::memory_order_relaxed) == 0)
	{
		return false;
	}
	Task task = popTask();
	if (!task.function)
	{
		return false;
	}
	runTask(task);
	return true;
}

bool ThreadPool::executeGroupTask(TaskGroup& group)
{
	Task task;
	{
		std::lock_guard<std::mutex> lock(m_workers->mutex);
		for (const std::unique_ptr<TaskDomain>& domain : m_domains)
		{
			const auto it = std::find_if(domain->tasks.begin(), domain->tasks.end(), [&](const Task& t) { return t.group == &group; });
			if (it != domain->tasks.end())
			{
				task = std::move(*it);
				domain->tasks.erase(it);
				m_workers->queuedTasks.fetch_sub(1, std::memory_order_relaxed);
				break;
			}
		}
	}
	if (!task.function)
	{
		return false;
	}
	runTask(task);
	return true;
}

void ThreadPool::runParallel(const ParallelJob& job) noexcept
{
	// Lanes hold 32-bit offsets
	constexpr u64 kSegment = 0xffffffffu;
	if (job.count <= kSegment)
	{
		runParallelSegment(job);
		return;
	}
	for (u64 first = 0; first < job.count; first += kSegment)
	{
		struct Part
		{
			const ParallelJob* job;
			u64 offset;
		};
		const Part part{&job, first};
		const ParallelJob segment{[](const void* p, u64 a, u64 b)
			{
				const Part& part = *static_cast<const Part*>(p);
				part.job->run(part.job->context, part.offset + a, part.offset + b);
			},
			&part, std::min(kSegment, job.count - first)};
		runParallelSegment(segment);
	}
}

void ThreadPool::runParallelSegment(const ParallelJob& job) noexcept
{
	const u32 width = std::max(u32(std::min<u64>({job.count, u64(numWorkerThreads()) + 1, kMaxThreads})), 1u);
	if (width == 1)
	{
		job.run(job.context, 0, job.count);
		return;
	}

	u32 helpers[kMaxThreads];
	const u32 initial = job.count <= width ? width - 1 : std::min(width - 1, kInitialHelpers);
	const u32 got = m_workers->claim(ArrayView<u32>(helpers, initial));
	union Lanes
	{
		Lanes() {}
		Lane lane[kMaxThreads];
	} lanes;
	const u32 n = got + 1;
	for (u32 i = 0; i < width; ++i)
	{
		new (&lanes.lane[i]) Lane{i < n ? packRange(job.count * i / n, job.count * (i + 1) / n) : 0};
	}
	ParallelRun run{job.run, job.context, job.count, lanes.lane, width};
	run.reservedLaneCount.store(n, std::memory_order_relaxed);
	run.helpersWorking.store(got, std::memory_order_relaxed);
	run.helpersHoldingRun.store(got, std::memory_order_relaxed);
	for (u32 i = 0; i < got; ++i)
	{
		m_workers->ring(helpers[i], &run, i + 1);
	}
	workLanes(run, 0, *m_workers);
	if (!spinFor(m_desc.joinSpinUs, [&]() { return run.helpersWorking.load(std::memory_order_acquire) == 0; }))
	{
		for (u32 v = run.helpersWorking.fetch_or(kJoinWaiting, std::memory_order_acq_rel) | kJoinWaiting; v != kJoinWaiting;
			 v = run.helpersWorking.load(std::memory_order_acquire))
		{
			waitOnWord(run.helpersWorking, v);
		}
	}
	for (u32 i = 0; run.helpersHoldingRun.load(std::memory_order_acquire) != 0; ++i)
	{
		if (i < 256)
		{
			pauseCpu();
		}
		else
		{
			std::this_thread::yield();
		}
	}
}

void TaskGroup::wait()
{
	while (m_pending.load() != 0 && m_pool.executeGroupTask(*this))
	{
	}
	if (spinFor(m_pool.m_desc.joinSpinUs, [this]() { return m_pending.load(std::memory_order_acquire) == 0; }))
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return;
	}
	std::unique_lock<std::mutex> lock(m_mutex);
	m_finishedCondition.wait(lock, [this]() { return m_pending.load() == 0; });
}

void TaskGroup::taskFinished()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (--m_pending == 0)
	{
		m_finishedCondition.notify_all();
	}
}

Semaphore::Semaphore(ThreadPool& pool, u32 maxCount)
	: m_pool(pool)
	, m_native(std::min<u32>(maxCount, 1 + pool.numWorkerThreads()))
{
}

void Semaphore::acquire(bool allowTaskExecution)
{
	if (allowTaskExecution)
	{
		while (!m_native.try_acquire())
		{
			if (!m_pool.tryExecuteTask())
			{
				m_native.acquire();
				return;
			}
		}
	}
	else
	{
		m_native.acquire();
	}
}
void Semaphore::release()
{
	m_native.release();
}

namespace
{
std::mutex g_globalMutex;
ThreadPool::Desc g_globalDesc = {.workers = ThreadPool::Desc::kHardwareWorkers};
bool g_globalStarted = false;

ThreadPool::Desc startGlobal()
{
	std::lock_guard<std::mutex> lock(g_globalMutex);
	g_globalStarted = true;
	return g_globalDesc;
}
} // namespace

void ThreadPool::setGlobalDesc(const Desc& desc)
{
	std::lock_guard<std::mutex> lock(g_globalMutex);
	RUSH_ASSERT_MSG(!g_globalStarted, "setGlobalDesc after the global pool started");
	if (!g_globalStarted)
	{
		g_globalDesc = desc;
	}
}

ThreadPool& ThreadPool::global()
{
	static ThreadPool pool(startGlobal());
	return pool;
}

} // namespace Rush
