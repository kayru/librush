#include "UtilThreadPool.h"
#include "UtilLog.h"

#include <algorithm>
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
	m_shutdown = true;
	m_wakeCondition.notify_all();
	for (std::thread& t : m_threads)
	{
		t.join();
	}
}

ThreadPool::TaskFunction ThreadPool::popTask(bool waitForSignal)
{
	std::unique_lock<std::mutex> lock(m_mutex);

	if (waitForSignal)
	{
		m_wakeCondition.wait(lock, [this]() { return m_shutdown.load() || !m_tasks.empty(); });
	}

	TaskFunction result;
	if (!m_tasks.empty())
	{
		result = std::move(m_tasks.back());
		m_tasks.pop_back();
	}
	return result;
}

void ThreadPool::pushTask(TaskFunction&& fun, bool allowImmediateExecution)
{
	if (m_threads.empty() || (m_runningTasks.load() >= numWorkerThreads() && allowImmediateExecution))
	{
		fun();
	}
	else
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_tasks.push_back(std::move(fun));
		m_wakeCondition.notify_one();
	}
}

bool ThreadPool::doWorkInternal(bool waitForSignal)
{
	TaskFunction task = popTask(waitForSignal);
	if (task)
	{
		++m_runningTasks;
		task();
		--m_runningTasks;
		return true;
	}
	return false;
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
			m_pool.tryExecuteTask();
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
