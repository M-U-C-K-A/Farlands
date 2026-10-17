// -----------------------------------------------------------------------------
// Fichier : job_system.h
// Rôle : Pool de threads minimal avec file de priorité (plus petite priorité
// = exécutée en premier). Utilisé pour générer et mailler les chunks hors du
// thread principal.
// -----------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class JobSystem
{
  public:
	explicit JobSystem(unsigned threadCount);
	~JobSystem();

	JobSystem(const JobSystem &) = delete;
	JobSystem &operator=(const JobSystem &) = delete;

	/// Ajoute un travail ; priority faible = prioritaire
	void submit(float priority, std::function<void()> job);

	/// Nombre de travaux en attente (non démarrés)
	size_t pending() const;
	unsigned threadCount() const
	{
		return static_cast<unsigned>(m_threads.size());
	}

  private:
	struct Job
	{
		float priority;
		uint64_t order; // FIFO à priorité égale
		std::function<void()> fn;
		bool operator<(const Job &o) const
		{
			if(priority != o.priority)
				return priority > o.priority;
			return order > o.order;
		}
	};

	void workerLoop();

	std::vector<std::thread> m_threads;
	std::priority_queue<Job> m_queue;
	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	uint64_t m_counter = 0;
	bool m_stop = false;
};
