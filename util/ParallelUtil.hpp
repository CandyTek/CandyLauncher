#pragma once

#include <algorithm>
#include <thread>
#include <vector>

// 将 [0, total) 均分给多个线程执行 fn(begin, end)，当前线程也参与计算
// 每个线程至少分到 minPerThread 个元素，数量较少时直接在当前线程执行
// fn 内部不应抛出异常
template <class Fn>
void ParallelForRange(const size_t total, const size_t minPerThread, Fn&& fn) {
	if (total == 0) return;
#ifdef _DEBUG
	// Debug CRT 堆所有分配共用一把全局锁，多线程只会互相等待，反而更慢
	fn(size_t{0}, total);
	return;
#endif
	const size_t hw = (std::max)(1u, std::thread::hardware_concurrency());
	const size_t perThread = (std::max)(size_t{1}, minPerThread);
	const size_t threadCount = (std::min)(hw, (total + perThread - 1) / perThread);
	if (threadCount <= 1) {
		fn(size_t{0}, total);
		return;
	}

	const size_t chunk = (total + threadCount - 1) / threadCount;
	std::vector<std::thread> workers;
	workers.reserve(threadCount - 1);
	for (size_t t = 1; t < threadCount; ++t) {
		const size_t begin = t * chunk;
		const size_t end = (std::min)(begin + chunk, total);
		if (begin >= end) break;
		workers.emplace_back([&fn, begin, end] { fn(begin, end); });
	}
	fn(size_t{0}, (std::min)(chunk, total));
	for (auto& worker : workers) {
		worker.join();
	}
}
