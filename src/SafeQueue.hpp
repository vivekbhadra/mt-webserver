#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class SafeQueue
{
    std::queue<T>           task_queue;
    std::mutex              mtx;
    std::condition_variable cv;

  public:
    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lk(mtx);
            task_queue.push(std::move(item));
        }
        cv.notify_one();
    }

    // Blocking pop; returns false if shut down with empty queue
    bool pop(T &out, std::atomic<bool> &running)
    {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] { return !task_queue.empty() || !running; });
        if (task_queue.empty())
            return false;
        out = std::move(task_queue.front());
        task_queue.pop();
        return true;
    }

    void wake_all() { cv.notify_all(); }

    size_t size()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return task_queue.size();
    }
};
