#include "UtilThreadPool.h"
#include "UtilLog.h"

#include <algorithm>
#include <iterator>
#include <mutex>

namespace Rush
{

void ThreadPool::startWorkers(u32 numWorkers)
{
	RUSH_ASSERT(numWorkers <= kMaxThreads);
	std::unique_lock<std::mutex> lock(m_mutex);
	while (m_threads.size() < numWorkers)
	{
		m_threads.emplace_back([this]() {
			while (doWorkInternal(true)) {}
		});
	}
}

ThreadPool::~ThreadPool()
{
	{
		// Under the lock: a worker between its predicate check and its wait would miss the wakeup
		std::lock_guard<std::mutex> lock(m_mutex);
		m_shutdown = true;
	}
	m_wakeCondition.notify_all();
	for (std::thread& t : m_threads)
	{
		t.join();
	}
}

ThreadPool::Task ThreadPool::popTask(bool waitForSignal)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	if (waitForSignal)
	{
		m_wakeCondition.wait(lock, [this]() { return m_shutdown || !m_tasks.empty(); });
	}

	Task result;
	if (!m_tasks.empty())
	{
		result = std::move(m_tasks.back());
		m_tasks.pop_back();
	}
	return result;
}

void ThreadPool::pushTask(TaskFunction&& fun, bool allowImmediateExecution)
{
	pushTask(Task{std::move(fun), nullptr}, allowImmediateExecution);
}

void ThreadPool::pushTask(Task&& task, bool allowImmediateExecution)
{
	if (m_threads.empty() || (m_runningTasks.load() >= numWorkerThreads() && allowImmediateExecution))
	{
		runTask(task);
	}
	else
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_tasks.push_back(std::move(task));
		}
		m_wakeCondition.notify_one();
	}
}

void ThreadPool::runTask(Task& task)
{
	++m_runningTasks;
	task.function();
	task.function = nullptr;
	--m_runningTasks;
	if (task.group)
	{
		task.group->taskFinished();
	}
}

bool ThreadPool::doWorkInternal(bool waitForSignal)
{
	Task task = popTask(waitForSignal);
	if (task.function)
	{
		runTask(task);
		return true;
	}
	return false;
}

bool ThreadPool::executeGroupTask(TaskGroup& group)
{
	Task task;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const auto it = std::find_if(m_tasks.rbegin(), m_tasks.rend(), [&](const Task& t) { return t.group == &group; });
		if (it == m_tasks.rend())
		{
			return false;
		}
		task = std::move(*it);
		m_tasks.erase(std::next(it).base());
	}
	runTask(task);
	return true;
}

void TaskGroup::wait()
{
	while (m_pending.load() != 0 && m_pool.executeGroupTask(*this))
	{
	}
	// The rest are running on other threads. A task those start for this
	// group is picked up by a worker once one finishes.
	std::unique_lock<std::mutex> lock(m_mutex);
	m_finishedCondition.wait(lock, [this]() { return m_pending.load() == 0; });
}

void TaskGroup::taskFinished()
{
	// Notify under the lock: the waiter may destroy the group as soon as it
	// sees zero, and it can only do so after this unlock
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

ThreadPool& ThreadPool::global()
{
	static ThreadPool pool;
	static std::once_flag initFlag;
	std::call_once(initFlag, []() {
		u32 numThreads = std::max(1u, std::thread::hardware_concurrency()) - 1;
		pool.startWorkers(numThreads);
	});
	return pool;
}

} // namespace Rush
