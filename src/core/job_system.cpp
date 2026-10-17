#include "job_system.h"

JobSystem::JobSystem(unsigned threadCount)
{
	if(threadCount == 0)
		threadCount = 1;
	for(unsigned i = 0; i < threadCount; ++i)
		m_threads.emplace_back([this] { workerLoop(); });
}

JobSystem::~JobSystem()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stop = true;
		// Abandonner les travaux non démarrés
		while(!m_queue.empty())
			m_queue.pop();
	}
	m_cv.notify_all();
	for(auto &t : m_threads)
		t.join();
}

void JobSystem::submit(float priority, std::function<void()> job)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.push(Job{priority, m_counter++, std::move(job)});
	}
	m_cv.notify_one();
}

size_t JobSystem::pending() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_queue.size();
}

void JobSystem::workerLoop()
{
	for(;;)
		{
			std::function<void()> fn;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cv.wait(lock, [this] { return m_stop || !m_queue.empty(); });
				if(m_stop)
					return;
				fn = std::move(const_cast<Job &>(m_queue.top()).fn);
				m_queue.pop();
			}
			fn();
		}
}
