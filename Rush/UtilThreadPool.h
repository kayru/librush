#pragma once

#include "Rush.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <semaphore>

namespace Rush
{

class TaskGroup;

class ThreadPool
{
public:
	using TaskFunction = std::function<void()>;

	ThreadPool() = default;
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	static constexpr u32 kMaxThreads = 64;

	static ThreadPool& global();

	void startWorkers(u32 numWorkers);
	void pushTask(TaskFunction&& fun, bool allowImmediateExecution = true);
	bool tryExecuteTask() { return doWorkInternal(false); }

	bool executeTasksUntilIdle()
	{
		u64 numExecuted = 0;
		while (tryExecuteTask()) { ++numExecuted; }
		return numExecuted != 0;
	}

	u32 numWorkerThreads() const { return u32(m_threads.size()); }

private:
	friend class TaskGroup;

	struct Task
	{
		TaskFunction function;
		TaskGroup*   group = nullptr;
	};

	void pushTask(Task&& task, bool allowImmediateExecution);
	bool doWorkInternal(bool waitForSignal);
	bool executeGroupTask(TaskGroup& group);
	Task popTask(bool waitForSignal);
	void runTask(Task& task);

	std::vector<std::thread> m_threads;
	std::vector<Task> m_tasks;
	std::mutex m_mutex;
	std::condition_variable m_wakeCondition;
	bool m_shutdown = false;
	std::atomic<u32> m_runningTasks = 0;
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
					this},
				true);
		}
		else
		{
			fun();
		}
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
	if (begin >= end) return;

	ThreadPool& pool = ThreadPool::global();
	TaskGroup group(pool);
	for (I i = begin; i < end; ++i)
	{
		group.run([i, &fun]() { fun(i); });
	}
}

template <typename IT, typename F>
inline void parallelForEach(IT itBegin, IT itEnd, F fun)
{
	ThreadPool& pool = ThreadPool::global();
	TaskGroup group(pool);
	for (; itBegin != itEnd; ++itBegin)
	{
		auto* it = &(*itBegin);
		group.run([&fun, it]() { fun(*it); });
	}
}

template <typename T, typename F>
inline void parallelForEach(T& container, F fun)
{
	parallelForEach(std::begin(container), std::end(container), fun);
}

} // namespace Rush
