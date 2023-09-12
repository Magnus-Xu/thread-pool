#include "threadpool.hpp"

namespace ThreadPool
{
    join_threads::join_threads(std::vector<std::thread>& _threads) : threads(_threads) {}

    join_threads::~join_threads()
    {
        for (auto& thread : threads) {
            if (thread.joinable())
                thread.join();
        }
    }

    thread_pool::thread_pool(const unsigned int thread_num) : done(false), joiner(threads)
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

    thread_pool::~thread_pool()
    {
        done = true;
    }

    void thread_pool::run_pending_task()
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

    void thread_pool::worker_thread(unsigned int _my_index)
    {
        while (!done) {
            run_pending_task();
        }
    }
}
