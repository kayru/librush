#pragma once

#include "Rush.h"
#include "UtilArray.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <semaphore>

namespace Rush
{

class TaskGroup;

struct TaskDomain;
struct ThreadPoolWorkers;

class ThreadPool
{
public:
	using TaskFunction = std::function<void()>;

	struct Desc
	{
		struct Cpu
		{
			static constexpr u32 kMaxGroups = 64;

			u32 processorNumber = 0; // as per currentProcessorNumber()
			u8 group = 0; // last-level cache group, with close numbers for physically close caches
			u32 core = 0; // the physical core number
		};

		static constexpr u32 kHardwareWorkers = ~0u; // all hardware threads but the caller's

		u32 workers = 0; // how many threads to create
		u32 joinSpinUs = 50; // spin for this many microseconds before sleeping on join
		u32 minWorkPerHelper = 0; // in wake times, 0 means twice the hardware threads
		DynamicArray<Cpu> cpus; // optional description of CPU topology
	};

	struct ParallelJob
	{
		void (*run)(const void* context, u64 first, u64 last);
		const void* context;
		u64 count;
	};

	ThreadPool();
	explicit ThreadPool(const Desc& desc);
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	static constexpr u32 kMaxThreads = 64;

	static ThreadPool& global();
	static void setGlobalDesc(const Desc& desc); // optional config before first global()

	void startWorkers(u32 numWorkers);
	void pushTask(TaskFunction&& fun, bool allowImmediateExecution = true);

	TaskDomain* createDomain(int priority); // higher runs first
	TaskDomain* defaultDomain() { return m_defaultDomain; }
	static TaskDomain* currentDomain();
	static void setCurrentDomain(TaskDomain* domain);
	bool tryExecuteTask() { return doWorkInternal(); }

	bool executeTasksUntilIdle()
	{
		u64 numExecuted = 0;
		while (tryExecuteTask()) { ++numExecuted; }
		return numExecuted != 0;
	}

	static u32 currentProcessorNumber(); // ~0u where the OS doesn't say
	u32 numWorkerThreads() const;
	u32 numIdleWorkers() const;

	void runParallel(const ParallelJob& job) noexcept;

private:
	friend class TaskGroup;
	friend struct TaskDomain;

	struct Task
	{
		TaskFunction function;
		TaskGroup* group = nullptr;
		TaskDomain* domain = nullptr;
	};

	void pushTask(Task&& task, bool allowImmediateExecution);
	void pushTasks(TaskDomain* domain, DynamicArray<Task>&& tasks);
	bool doWorkInternal();
	bool executeGroupTask(TaskGroup& group);
	Task popTask();
	void runTask(Task& task) noexcept;
	TaskDomain* resolveDomain(TaskDomain* domain) const;
	TaskDomain* eligibleDomain();
	void workerMain(u32 index);
	void wakeForQueue(u32 count);
	void runParallelSegment(const ParallelJob& job) noexcept;

	Desc m_desc;
	DynamicArray<std::thread> m_threads;
	DynamicArray<std::unique_ptr<TaskDomain>> m_domains;
	TaskDomain* m_defaultDomain = nullptr;
	std::unique_ptr<ThreadPoolWorkers> m_workers;
};

class TaskDomainScope
{
public:
	explicit TaskDomainScope(TaskDomain* domain) : m_previous(ThreadPool::currentDomain()) { ThreadPool::setCurrentDomain(domain); }
	~TaskDomainScope() { ThreadPool::setCurrentDomain(m_previous); }
	TaskDomainScope(const TaskDomainScope&) = delete;
	TaskDomainScope& operator=(const TaskDomainScope&) = delete;

private:
	TaskDomain* m_previous;
};

class Semaphore
{
public:
	Semaphore(ThreadPool& pool, u32 maxCount);

	Semaphore(const Semaphore&) = delete;
	Semaphore& operator=(const Semaphore&) = delete;

	bool tryAcquire() { return m_native.try_acquire(); }
	void acquire(bool allowTaskExecution = true);
	void release();

private:
	ThreadPool& m_pool;
	std::counting_semaphore<ThreadPool::kMaxThreads + 1> m_native;
};

class TaskGroup
{
public:
	TaskGroup(ThreadPool& pool, Semaphore* concurrencyLimit = nullptr)
		: m_pool(pool), m_concurrencyLimit(concurrencyLimit) {}
	~TaskGroup() { wait(); }

	TaskGroup(const TaskGroup&) = delete;
	TaskGroup& operator=(const TaskGroup&) = delete;

	template <typename F>
	void run(F&& fun)
	{
		const bool acquired = m_concurrencyLimit && m_concurrencyLimit->tryAcquire();

		if (!m_concurrencyLimit || acquired)
		{
			++m_pending;
			m_pool.pushTask(
				ThreadPool::Task{
					[semaphore = acquired ? m_concurrencyLimit : nullptr, f = std::forward<F>(fun)]() mutable
					{
						f();
						if (semaphore)
						{
							semaphore->release();
						}
					},
					this,
					ThreadPool::currentDomain()},
				true);
		}
		else
		{
			fun();
		}
	}

	template <typename F>
	void runBatch(u32 count, F&& fun)
	{
		if (count == 0)
		{
			return;
		}
		auto shared = std::make_shared<std::decay_t<F>>(std::forward<F>(fun));
		if (m_concurrencyLimit)
		{
			for (u32 i = 0; i < count; ++i)
			{
				run([shared, i]() { (*shared)(i); });
			}
			return;
		}
		DynamicArray<ThreadPool::Task> tasks;
		tasks.reserve(count);
		for (u32 i = 0; i < count; ++i)
		{
			tasks.push_back(ThreadPool::Task{[shared, i]() { (*shared)(i); }, this, nullptr});
		}
		m_pending += count;
		m_pool.pushTasks(ThreadPool::currentDomain(), std::move(tasks));
	}

	void wait();

private:
	friend class ThreadPool;

	void taskFinished();

	ThreadPool& m_pool;
	Semaphore* m_concurrencyLimit = nullptr;
	std::mutex m_mutex;
	std::condition_variable m_finishedCondition;
	std::atomic<u32> m_pending = 0;
};

template <typename I, typename F>
inline void parallelFor(ThreadPool& pool, I begin, I end, F fun)
{
	static_assert(std::is_integral_v<I>);
	using U = std::make_unsigned_t<I>;
	if (begin >= end)
	{
		return;
	}
	struct Context
	{
		F* fun;
		I begin;
	};
	const Context context{&fun, begin};
	const ThreadPool::ParallelJob job{
		[](const void* p, u64 first, u64 last)
		{
			const Context& c = *static_cast<const Context*>(p);
			F& f = *c.fun;
			const I b = c.begin;
			for (u64 k = first; k < last; ++k)
			{
				f(I(U(U(b) + U(k))));
			}
		},
		&context, u64(U(U(end) - U(begin)))};
	pool.runParallel(job);
}

template <typename I, typename F>
inline void parallelFor(I begin, I end, F fun)
{
	parallelFor(ThreadPool::global(), begin, end, std::move(fun));
}

template <typename IT, typename F>
inline void parallelForEach(IT itBegin, IT itEnd, F fun)
{
	parallelFor(std::ptrdiff_t(0), std::ptrdiff_t(itEnd - itBegin), [&](std::ptrdiff_t i) { fun(itBegin[i]); });
}

template <typename T, typename F>
inline void parallelForEach(T& container, F fun)
{
	parallelForEach(std::begin(container), std::end(container), fun);
}

} // namespace Rush
