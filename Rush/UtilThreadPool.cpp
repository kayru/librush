#include "UtilThreadPool.h"
#include "UtilLog.h"

#include <algorithm>
#include <deque>
#include <mutex>

namespace Rush
{

struct TaskDomain
{
	int priority = 0;
	std::deque<ThreadPool::Task> tasks;
};

namespace
{
thread_local TaskDomain* t_currentDomain = nullptr;
}

ThreadPool::ThreadPool()
{
	m_defaultDomain = createDomain({});
}

TaskDomain* ThreadPool::createDomain(const TaskDomainDesc& desc)
{
	auto domain = std::make_unique<TaskDomain>();
	domain->priority = desc.priority;
	TaskDomain* result = domain.get();
	std::lock_guard<std::mutex> lock(m_mutex);
	const auto at = std::find_if(m_domains.begin(), m_domains.end(), [&](const std::unique_ptr<TaskDomain>& p) { return p->priority <= desc.priority; });
	RUSH_ASSERT_MSG(at == m_domains.end() || (*at)->priority != desc.priority, "Two task domains with priority %d", desc.priority);
	m_domains.insert(at, std::move(domain));
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

void ThreadPool::startWorkers(u32 numWorkers)
{
	RUSH_ASSERT(numWorkers <= kMaxThreads);
	std::unique_lock<std::mutex> lock(m_mutex);
	while (m_threads.size() < numWorkers)
	{
		m_threads.emplace_back([this]() {
			while (doWorkInternal(true))
			{
			}
		});
	}
	m_workerCount = u32(m_threads.size());
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
		m_wakeCondition.wait(lock, [this]() { return m_shutdown || eligibleDomain() != nullptr; });
	}

	Task result;
	if (TaskDomain* domain = eligibleDomain())
	{
		result = std::move(domain->tasks.front());
		domain->tasks.pop_front();
	}
	return result;
}

void ThreadPool::pushTask(TaskFunction&& fun, bool allowImmediateExecution)
{
	pushTask(Task{std::move(fun), nullptr, currentDomain()}, allowImmediateExecution);
}

void ThreadPool::pushTask(Task&& task, bool allowImmediateExecution)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	task.domain = resolveDomain(task.domain);
	if (m_threads.empty() || m_shutdown || (m_runningTasks.load() >= m_threads.size() && allowImmediateExecution))
	{
		lock.unlock();
		runTask(task);
	}
	else
	{
		task.domain->tasks.push_back(std::move(task));
		lock.unlock();
		m_wakeCondition.notify_one();
	}
}

void ThreadPool::pushTasks(TaskDomain* domain, std::vector<Task>&& tasks)
{
	if (tasks.empty())
	{
		return;
	}
	std::unique_lock<std::mutex> lock(m_mutex);
	domain = resolveDomain(domain);
	const bool runHere = m_threads.empty() || m_shutdown;
	for (Task& task : tasks)
	{
		task.domain = domain;
		if (!runHere)
		{
			domain->tasks.push_back(std::move(task));
		}
	}
	lock.unlock();
	if (!runHere)
	{
		m_wakeCondition.notify_all();
		return;
	}
	for (Task& task : tasks)
	{
		runTask(task);
	}
}

void ThreadPool::runTask(Task& task) noexcept
{
	++m_runningTasks;
	TaskDomain* const previous = t_currentDomain;
	t_currentDomain = task.domain;
	task.function();
	task.function = nullptr;
	t_currentDomain = previous;
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
		for (const std::unique_ptr<TaskDomain>& domain : m_domains)
		{
			const auto it = std::find_if(domain->tasks.begin(), domain->tasks.end(), [&](const Task& t) { return t.group == &group; });
			if (it != domain->tasks.end())
			{
				task = std::move(*it);
				domain->tasks.erase(it);
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
