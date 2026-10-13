#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// 跟踪插件发起的后台线程。Shutdown 时拒绝新任务、唤醒可取消的等待并 join 全部线程，
// 保证主程序 FreeLibrary 之前没有线程还在执行本 DLL 里的代码（detach 的线程做不到这一点）。
// 进程经 ExitProcess 退出时不会调用 Shutdown，此时其他线程已被系统结束，析构只能 detach，否则 join 会永久阻塞。
class BackgroundWorkTracker {
public:
	BackgroundWorkTracker() = default;
	BackgroundWorkTracker(const BackgroundWorkTracker&) = delete;
	BackgroundWorkTracker& operator=(const BackgroundWorkTracker&) = delete;

	~BackgroundWorkTracker() {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& entry : m_threads) {
			if (entry.thread.joinable()) entry.thread.detach();
		}
		m_threads.clear();
	}

	// 重新开始接受任务（插件 Initialize 时调用，同一个 DLL 实例再次初始化时清掉上次的关闭状态）
	void Reset() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stopping = false;
		m_cancel = false;
	}

	// 在新线程上执行 work；正在关闭时不启动并返回 false
	bool Run(std::function<void()> work) {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_stopping) return false;
		Reap();
		auto done = std::make_shared<std::atomic<bool>>(false);
		Entry entry;
		entry.done = done;
		entry.thread = std::thread([work = std::move(work), done]() {
			try {
				work();
			} catch (...) {
			}
			*done = true;
		});
		m_threads.push_back(std::move(entry));
		return true;
	}

	bool IsCancelled() const {
		return m_cancel.load();
	}

	// 可取消的等待：等满 milliseconds 返回 true，期间被 Shutdown 取消返回 false
	bool Wait(const unsigned milliseconds) {
		std::unique_lock<std::mutex> lock(m_mutex);
		return !m_cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return m_cancel.load(); });
	}

	// 请求取消并等待全部线程结束；之后 Run 一律返回 false，直到 Reset
	void Shutdown() {
		std::vector<Entry> entries;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_stopping = true;
			m_cancel = true;
			entries.swap(m_threads);
		}
		m_cv.notify_all();
		for (auto& entry : entries) {
			if (entry.thread.joinable()) entry.thread.join();
		}
	}

private:
	struct Entry {
		std::thread thread;
		std::shared_ptr<std::atomic<bool>> done;
	};

	// 回收已经结束的线程；调用方需持有 m_mutex
	void Reap() {
		for (auto it = m_threads.begin(); it != m_threads.end();) {
			if (it->done && it->done->load()) {
				if (it->thread.joinable()) it->thread.join();
				it = m_threads.erase(it);
			} else {
				++it;
			}
		}
	}

	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<Entry> m_threads;
	bool m_stopping = false;
	std::atomic<bool> m_cancel{false};
};
