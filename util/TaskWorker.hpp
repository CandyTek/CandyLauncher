#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class TaskWorker {
public:
	TaskWorker() = default;

	void init() {
		m_shouldStop = false;
		m_workerThread = std::thread(&TaskWorker::workerLoop, this);
	}

	~TaskWorker() {
		stop();
	}

	// 禁止拷贝，允许移动
	TaskWorker(const TaskWorker&) = delete;
	TaskWorker& operator=(const TaskWorker&) = delete;
	TaskWorker(TaskWorker&&) = default;
	TaskWorker& operator=(TaskWorker&&) = default;

	// 提交任务
	void submit(std::function<void()> task) {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_tasks.push(std::move(task));
		}
		m_cv.notify_one();
	}

	// 停止并等待线程结束
	void stop() {
		try {
			if (!m_shouldStop.exchange(true)) {
				// 确保只停止一次
				{
					std::lock_guard<std::mutex> lock(m_mutex);
				}
				m_cv.notify_all();

				if (m_workerThread.joinable()) {
					m_workerThread.join();
				}
			}
		} catch (...) {
		}
	}

private:
	void workerLoop() {
		while (!m_shouldStop) {
			std::function<void()> task;

			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cv.wait(lock, [&]() {
					return !m_tasks.empty() || m_shouldStop;
				});

				if (m_shouldStop) break;

				task = std::move(m_tasks.front());
				m_tasks.pop();
			}

			try {
				task();
			} catch (...) {
				// 捕获异常，防止线程崩溃
			}
		}
	}

private:
	std::queue<std::function<void()>> m_tasks;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::thread m_workerThread;
	std::atomic<bool> m_shouldStop;
};
