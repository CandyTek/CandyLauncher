#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <atomic>


class ThreadPool {
public:
    /**
     * @brief 构造函数，创建并启动指定数量的工作线程
     * @param threads 要创建的线程数量
     */
    explicit ThreadPool(size_t threads);

    /**
     * @brief 析构函数，等待所有任务完成并销毁线程池
     */
    ~ThreadPool();

    /**
     * @brief 向任务队列中添加一个新任务
     * @tparam F 函数类型
     * @tparam Args 函数参数类型
     * @param f 要执行的函数
     * @param args 函数的参数
     * @return 返回一个 std::future，用于获取任务的返回值
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

private:
    // 工作线程的容器
    std::vector<std::thread> workers;
    // 任务队列
    std::queue<std::function<void()>> tasks;

    // 同步原语
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

// --- 实现 ---

inline ThreadPool::ThreadPool(size_t threads) : stop(false) {
    if (threads == 0) {
        // 至少保证一个线程
        threads = 1; 
    }
    for (size_t i = 0; i < threads; ++i) {
        // 创建工作线程，并立即开始执行它们的循环
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;

                { // 临界区开始
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    
                    // 等待条件：任务队列不为空 或 线程池被停止
                    this->condition.wait(lock, [this] { 
                        return this->stop || !this->tasks.empty(); 
                    });

                    // 如果线程池被停止且任务队列已空，则退出循环
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }

                    // 从队列中取出一个任务
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                } // 临界区结束，锁自动释放

                // 执行任务
                task();
            }
        });
    }
}


template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;

    // 使用 packaged_task 来包装任务，以便获取其 future
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // 不允许在线程池停止后继续添加任务
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        // 将执行 packaged_task 的 lambda 放入任务队列
        tasks.emplace([task]() { (*task)(); });
    }
    // 唤醒一个等待的工作线程
    condition.notify_one();
    return res;
}


inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    // 唤醒所有线程，让它们检查 stop 标志并退出
    condition.notify_all();

    // 等待所有工作线程执行完毕
    for (std::thread& worker : workers) {
        worker.join();
    }
}

