// MT25022 - J Chandra Harsha
// Thread Pool implementation for reusable thread management

#ifndef MT25022_THREADPOOL_H
#define MT25022_THREADPOOL_H

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <memory>
#include <type_traits>

/**
 * @brief A thread pool that reuses a fixed set of worker threads.
 * 
 * Demonstrates RAII (threads are joined on destruction), templates
 * (for flexible return types via submit()), and move semantics.
 */
class ThreadPool {
private:
    std::vector<std::thread> workers_;        ///< Worker threads
    std::queue<std::function<void()>> tasks_;  ///< Task queue
    std::mutex queueMutex_;                   ///< Protects the task queue
    std::condition_variable condition_;         ///< Notifies workers of new tasks
    bool stop_;                                ///< Signal to shut down
    int numThreads_;                           ///< Number of worker threads

public:
    /**
     * @brief Create a thread pool with the specified number of threads.
     * @param numThreads Number of worker threads (must be > 0)
     * @throws std::invalid_argument if numThreads <= 0
     */
    explicit ThreadPool(int numThreads);
    
    /**
     * @brief Destructor: signals all threads to stop and joins them.
     * Demonstrates RAII - resources cleaned up automatically.
     */
    ~ThreadPool();
    
    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    /**
     * @brief Submit a task to the pool.
     * 
     * Uses templates to accept any callable and return a future
     * with the result type.
     * 
     * @tparam F Callable type
     * @tparam Args Argument types
     * @param f The function to execute
     * @param args Arguments to pass to f
     * @return std::future with the return value of f
     * @throws std::runtime_error if pool has been stopped
     */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        
        using ReturnType = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_) {
                throw std::runtime_error("ThreadPool: cannot submit to stopped pool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    /** @brief Get the number of threads in the pool. */
    int getNumThreads() const;
    
    /** @brief Get the number of pending tasks. */
    size_t pendingTasks() const;
};

#endif // MT25022_THREADPOOL_H
