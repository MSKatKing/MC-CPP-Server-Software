//
// Created by wait4 on 4/7/2024.
//

#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


class ThreadPool {
public:
    explicit ThreadPool(const size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    template<class F, class... Args>
    void enqueue(F&& f, Args&&... args) {
        {
            std::unique_lock lock(queueMutex);
            tasks.emplace([f, args...] { f(args...); });
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.detach();
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};



#endif //THREAD_POOL_H
