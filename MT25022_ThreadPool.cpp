// MT25022 - J Chandra Harsha
// Thread Pool implementation

#include "MT25022_ThreadPool.h"

ThreadPool::ThreadPool(int numThreads) : stop_(false), numThreads_(numThreads) {
    if (numThreads <= 0) {
        throw std::invalid_argument("ThreadPool: number of threads must be > 0, got " + 
                                    std::to_string(numThreads));
    }
    
    for (int i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex_);
                    this->condition_.wait(lock, [this]() {
                        return this->stop_ || !this->tasks_.empty();
                    });
                    if (this->stop_ && this->tasks_.empty()) {
                        return;
                    }
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

int ThreadPool::getNumThreads() const {
    return numThreads_;
}

size_t ThreadPool::pendingTasks() const {
    return tasks_.size();
}
