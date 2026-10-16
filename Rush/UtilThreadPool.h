#pragma once

#include "Rush.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>
#include <semaphore>

namespace Rush
{

class TaskGroup;

struct TaskDomain;

struct TaskDomainDesc
{
	int priority = 0; // higher runs first; unique per pool, the default domain's is 0
};

class ThreadPool
{
public:
	using TaskFunction = std::function<void()>;

	ThreadPool();
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	static constexpr u32 kMaxThreads = 64;

	static ThreadPool& global();
	static void setGlobalWorkerCount(u32 numWorkers);

	void startWorkers(u32 numWorkers);
	void pushTask(TaskFunction&& fun, bool allowImmediateExecution = true);

	TaskDomain* createDomain(const TaskDomainDesc& desc);
	TaskDomain* defaultDomain() { return m_defaultDomain; }
	static TaskDomain* currentDomain();
	static void setCurrentDomain(TaskDomain* domain);
	bool tryExecuteTask() { return doWorkInternal(false); }

	bool executeTasksUntilIdle()
	{
		u64 numExecuted = 0;
		while (tryExecuteTask()) { ++numExecuted; }
		return numExecuted != 0;
	}

	u32 numWorkerThreads() const { return m_workerCount.load(); }

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
	void pushTasks(TaskDomain* domain, std::vector<Task>&& tasks);
	bool doWorkInternal(bool waitForSignal);
	bool executeGroupTask(TaskGroup& group);
	Task popTask(bool waitForSignal);
	void runTask(Task& task) noexcept;
	TaskDomain* resolveDomain(TaskDomain* domain) const;
	TaskDomain* eligibleDomain();

	std::vector<std::thread> m_threads;
	std::vector<std::unique_ptr<TaskDomain>> m_domains;
	TaskDomain* m_defaultDomain = nullptr;
	std::mutex m_mutex;
	std::condition_variable m_wakeCondition;
	bool m_shutdown = false;
	std::atomic<u32> m_runningTasks = 0;
	std::atomic<u32> m_workerCount = 0;
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

// wait() runs only this group's queued tasks, then blocks until the rest
// finish on other threads. A task's captures are destroyed before it counts
// as finished.
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
		std::vector<ThreadPool::Task> tasks;
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
inline void parallelFor(I begin, I end, F fun)
{
	static_assert(std::is_integral_v<I>);
	using U = std::make_unsigned_t<I>;
	if (begin >= end)
	{
		return;
	}
	ThreadPool& pool = ThreadPool::global();
	const u64 count = u64(U(U(end) - U(begin)));
	const u64 slots = u64(pool.numWorkerThreads()) + 1;
	const u32 tasks = u32(count < slots ? count : slots);
	if (tasks <= 1)
	{
		for (I i = begin; i < end; ++i)
		{
			fun(i);
		}
		return;
	}
	const u64 share = count / (u64(tasks) * 8);
	const u64 grain = share > 0 ? share : 1;
	std::atomic<u64> next = 0;
	TaskGroup group(pool);
	group.runBatch(tasks, [&](u32)
	{
		for (u64 first; (first = next.fetch_add(grain, std::memory_order_relaxed)) < count;)
		{
			const u64 last = first + grain < count ? first + grain : count;
			for (u64 k = first; k < last; ++k)
			{
				fun(I(U(U(begin) + U(k))));
			}
		}
	});
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
