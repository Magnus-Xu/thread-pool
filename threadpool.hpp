#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <cstdio>

// check if join_threads is joinable
class join_threads
{
public:
    explicit join_threads(std::vector<std::thread>& _threads) : threads(_threads) {}

    // RAII
    ~join_threads()
    {
        for (auto& thread : threads) {
            if (thread.joinable())
                thread.join();
        }
    }

private:
    std::vector<std::thread>& threads;
};

// queue with lock
template<typename T>
class thread_safe_queue
{
public:
    thread_safe_queue() {}
    thread_safe_queue(const thread_safe_queue& other) = delete;
    thread_safe_queue(thread_safe_queue&& other) = delete;
    thread_safe_queue& operator=(const thread_safe_queue& other) = delete;
    thread_safe_queue& operator=(thread_safe_queue&& other) = delete;

    void push(T new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(std::move(new_value));
        data_cond.notify_one();
    }

    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] {
            return !data_queue.empty();
            });
        value = std::move(data_queue.front());
        data_queue.pop();
    }

    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] {
            return !data_queue.empty();
            });
        std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue.front())));
        data_queue.pop();
        return res;
    }

    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return false;
        value = std::move(data_queue.front());
        data_queue.pop();
        return true;
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue.front())));
        data_queue.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }

private:
    mutable std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
};

class thread_pool
{
public:
    // initial thread pool
    thread_pool(const unsigned int thread_num) : done(false), joiner(threads)
    {
        // maximium thread number
        unsigned int const thread_count = std::min(std::thread::hardware_concurrency(), thread_num);
        try {
            for (unsigned i = 0; i < thread_count; ++i) {
                threads.emplace_back(std::thread(&thread_pool::worker_thread, this, i));
            }
        }
        catch (std::exception& ex) {
            done = true;
            throw std::runtime_error(ex.what());
        }
    }
    thread_pool(const thread_pool& other) = delete;
    thread_pool(thread_pool&& other) = delete;
    thread_pool& operator=(const thread_pool& other) = delete;
    thread_pool& operator=(thread_pool&& other) = delete;

    ~thread_pool()
    {
        done = true;
    }

    // input : f and args 
    template<typename Func, typename... Args>
    auto submit(Func&& f, Args&&...args) -> std::future<decltype(f(args...))>
    {
        using result_type = decltype(f(args...));

        // bind func with args, get lambda expression
        std::function<result_type()> func = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);
        auto task_ptr = std::make_shared<std::packaged_task<result_type()>>(func);
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
            };
        work_queue.push(std::move(warpper_func));
        return task_ptr->get_future();
    }

    // avoid deadlock because of waiting
    void run_pending_task()
    {
        std::function<void()> task;

        // local_queue before global_queue
        if (work_queue.try_pop(task)) {
            task();
        }
        else {
            std::this_thread::yield();
        }
    }
    
private:
    std::atomic_bool done;
    thread_safe_queue<std::function<void()>> work_queue;
    std::vector<std::thread> threads;
    join_threads joiner;

    // initial every thread
    void worker_thread(unsigned int _my_index)
    {
        while (!done) {
            run_pending_task();
        }
    }
};
