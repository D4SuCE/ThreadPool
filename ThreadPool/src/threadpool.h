#pragma once

#include <queue>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>

namespace details
{
	struct NonCopyable
	{
		NonCopyable() = default;
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator = (const NonCopyable&) = delete;
	};

	struct NotMoveable
	{
		NotMoveable() = default;
		NotMoveable(NotMoveable&&) = delete;
		NotMoveable& operator = (NotMoveable&&) = delete;
	};
}

namespace ThreadPool
{
	class ThreadPool : public details::NonCopyable, public details::NotMoveable
	{
	private:
		std::queue<std::function<void()>> m_tasks;
		std::vector<std::thread> m_threads;
		mutable std::mutex m_mutex;
		size_t m_threadsCount;
		std::atomic<size_t> m_tasksTotalCount = 0u;
		std::atomic<bool> m_running = true;
		std::atomic<bool> m_paused = false;

	public:
		explicit ThreadPool(size_t threadsCount = std::thread::hardware_concurrency())
			: m_threadsCount(threadsCount ? threadsCount : std::thread::hardware_concurrency()),
			m_threads(threadsCount ? threadsCount : std::thread::hardware_concurrency())
		{
			createThreads();
		}

		~ThreadPool()
		{
			waitForTasks();
			m_running = false;
			joinThreads();
		}

		void setPaused(bool paused)
		{
			m_paused = paused;
		}

		size_t getTasksQueuedCount() const
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_tasks.size();
		}

		size_t getTasksRunningCount() const
		{
			return m_tasksTotalCount - getTasksQueuedCount();
		}

		size_t getTasksTotalCount() const
		{
			return m_tasksTotalCount;
		}

		size_t getThreadsCount() const
		{
			return m_threadsCount;
		}

		template<typename F>
		void addTask(const F& task)
		{
			++m_tasksTotalCount;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_tasks.push(std::function<void()>(task));
			}
		}

		template<typename F, typename... T>
		void addTask(const F& task, T&&... args)
		{
			addTask(
				[task, args...]
				{ 
					task(args...); 
				}
			);
		}

		void waitForTasks()
		{
			while (true)
			{
				if (!m_paused)
				{
					if (m_tasksTotalCount == 0)
					{
						break;
					}
				}
				else
				{
					if (getTasksRunningCount() == 0)
					{
						break;
					}
				}
				std::this_thread::yield();
			}
		}

		void reset(size_t threadsCount = std::thread::hardware_concurrency())
		{
			const bool wasPaused = m_paused;
			m_paused = true;
			waitForTasks();
			m_running = false;
			joinThreads();

			m_threadsCount = threadsCount ? threadsCount : std::thread::hardware_concurrency();
			m_threads = std::vector<std::thread>(threadsCount);
			m_paused = false;
			m_running = true;
			createThreads();
		}

	private:
		void createThreads()
		{
			for (size_t i = 0u; i < m_threadsCount; i++)
			{
				m_threads[i] = std::thread(&ThreadPool::worker, this);
			}
		}

		void joinThreads()
		{
			for (size_t i = 0u; i < m_threadsCount; i++)
			{
				if (m_threads[i].joinable())
				{
					m_threads[i].join();
				}
			}
		}

		bool popTask(std::function<void()>& task)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (!m_tasks.empty())
			{
				task = std::move(m_tasks.front());
				m_tasks.pop();
				return true;
			}
			else
			{
				return false;
			}
		}

		void worker()
		{
			while (m_running)
			{
				std::function<void()> task;
				if (!m_paused && popTask(task))
				{
					task();
					--m_tasksTotalCount;
				}
				else
				{
					std::this_thread::yield();
				}
			}
		}
	};
}