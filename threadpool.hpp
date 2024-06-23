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

namespace ThreadPool
{
    /*
    * @brief check if join_threads is joinable
    */
class join_threads
{
public:
        explicit join_threads(std::vector<std::thread>& _threads);

    // RAII
        ~join_threads();

private:
    std::vector<std::thread>& threads;
};

    /*
    * @brief thread safe queue with lock
    */
template<typename T>
class thread_safe_queue
{
public:
        thread_safe_queue() = default;
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
        /*
        * @brief initial thread pool
        */
        thread_pool(const unsigned int thread_num);
    thread_pool(const thread_pool& other) = delete;
    thread_pool(thread_pool&& other) = delete;
    thread_pool& operator=(const thread_pool& other) = delete;
    thread_pool& operator=(thread_pool&& other) = delete;
        ~thread_pool();

        /*
        * @brief submit task
        * @param f: function
        * @param args: parameter
        * @return return future
        */
    template<typename Func, typename... Args>
        std::future<std::invoke_result_t<Func, Args...>> submit(Func&& f, Args&&...args)
    {
            // using result_type = decltype(f(args...));
            using result_type = std::invoke_result_t<Func, Args...>;

        // bind func with args, get lambda expression
        std::function<result_type()> func = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);
        auto task_ptr = std::make_shared<std::packaged_task<result_type()>>(func);
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
            };
        work_queue.push(std::move(warpper_func));
        return task_ptr->get_future();
    }

        /*
        * @brief avoid deadlock because of waiting
        */
        void run_pending_task();
    
private:
    std::atomic_bool done;
    thread_safe_queue<std::function<void()>> work_queue;
    std::vector<std::thread> threads;
    join_threads joiner;

        /*
        * @brief initial every thread
        */
        void worker_thread(unsigned int _my_index);
    };
};
