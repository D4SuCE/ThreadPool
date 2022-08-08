#include <string>
#include <iostream>
#include "threadpool.h"

namespace unit_tests {

    namespace details {

        void check(bool condition, const std::string& text, const std::string& func)
        {
            if (condition) {
                std::cout << func << ": " << text << " -> PASSED!\n";
            }
            else {
                std::cerr << func << ": " << text << "-> FAILED!\n";
            }
        }

    }

#define CHECK(condition, text) \
    details::check((condition), (text), __func__)

    void checkThreadPoolInitialization()
    {
        ThreadPool::ThreadPool thread_pool;
        CHECK(thread_pool.getThreadsCount() == std::thread::hardware_concurrency(), "check threads count");
        CHECK(thread_pool.getTasksTotalCount() == 0u, "check total tasks count");
        CHECK(thread_pool.getTasksQueuedCount() == 0u, "check tasks queued count");
        CHECK(thread_pool.getTasksRunningCount() == 0u, "check running tasks count");
    }

    void checkThreadPoolReset()
    {
        ThreadPool::ThreadPool thread_pool;
        thread_pool.reset(std::thread::hardware_concurrency() / 2);
        CHECK(thread_pool.getThreadsCount() == std::thread::hardware_concurrency() / 2, "check reset half count cores");
        thread_pool.reset(std::thread::hardware_concurrency());
        CHECK(thread_pool.getThreadsCount() == std::thread::hardware_concurrency(), "check reset full count cores");
    }

    void checkThreadPoolAddTasks()
    {
        ThreadPool::ThreadPool thread_pool;
        {
            bool flag = false;
            thread_pool.addTask(
                [&flag]
                {
                    flag = true;
                }
                );
            thread_pool.waitForTasks();
            CHECK(flag, "check add task without arguments");
        }
        {
            bool flag = false;
            thread_pool.addTask(
                [](bool* flag)
                {
                    *flag = true;
                },
                &flag
                    );
            thread_pool.waitForTasks();
            CHECK(flag, "check add task with single argument");
        }
        {
            bool flag1 = false;
            bool flag2 = false;
            thread_pool.addTask(
                [](bool* flag1, bool* flag2)
                {
                    *flag1 = *flag2 = true;
                },
                &flag1,
                    &flag2
                    );
            thread_pool.waitForTasks();
            CHECK(flag1 && flag2, "check add task with double arguments");
        }
    }

    void checkThreadPoolWaitForTasks()
    {
        ThreadPool::ThreadPool thread_pool;
        const size_t count_tasks = thread_pool.getThreadsCount() * 10;
        std::vector<std::atomic<bool>> flags(count_tasks);

        for (size_t i = 0u; i < count_tasks; ++i) {
            thread_pool.addTask(
                [&flags, i]
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    flags[i] = true;
                }
                );
        }
        thread_pool.waitForTasks();

        bool all_flags = true;
        for (size_t i = 0u; i < count_tasks; ++i) {
            all_flags &= flags[i];
        }

        CHECK(all_flags, "check waiting for all tasks");
    }

    void checkThreadPoolPausing()
    {
        const size_t count_tasks = std::min<size_t>(std::thread::hardware_concurrency(), 4u);
        ThreadPool::ThreadPool thread_pool(count_tasks);
        thread_pool.setPaused(true);

        for (size_t i = 0u; i < count_tasks * 3u; ++i) {
            thread_pool.addTask(
                []
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                );
        }

        CHECK(thread_pool.getTasksTotalCount() == count_tasks * 3u &&
            thread_pool.getTasksRunningCount() == 0u &&
            thread_pool.getTasksQueuedCount() == count_tasks * 3u,
            "check count tasks in pause");

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CHECK(thread_pool.getTasksTotalCount() == count_tasks * 3u &&
            thread_pool.getTasksRunningCount() == 0u &&
            thread_pool.getTasksQueuedCount() == count_tasks * 3u,
            "check count tasks in pause after some time");

        thread_pool.setPaused(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CHECK(thread_pool.getTasksTotalCount() == count_tasks * 2u &&
            thread_pool.getTasksRunningCount() == count_tasks &&
            thread_pool.getTasksQueuedCount() == count_tasks,
            "check count tasks in non pause");

        thread_pool.setPaused(true);
        thread_pool.waitForTasks();
        CHECK(thread_pool.getTasksTotalCount() == count_tasks &&
            thread_pool.getTasksRunningCount() == 0u &&
            thread_pool.getTasksQueuedCount() == count_tasks,
            "check count tasks in pause after in non pause");

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CHECK(thread_pool.getTasksTotalCount() == count_tasks &&
            thread_pool.getTasksRunningCount() == 0u &&
            thread_pool.getTasksQueuedCount() == count_tasks,
            "check count tasks in pause after in non pause after some time");

        thread_pool.setPaused(false);
        thread_pool.waitForTasks();
        CHECK(thread_pool.getTasksTotalCount() == 0u &&
            thread_pool.getTasksRunningCount() == 0u &&
            thread_pool.getTasksQueuedCount() == 0u,
            "check count wait for tasks in non pause");
    }

}

int main()
{
    unit_tests::checkThreadPoolInitialization();
    unit_tests::checkThreadPoolReset();
    unit_tests::checkThreadPoolAddTasks();
    unit_tests::checkThreadPoolWaitForTasks();
    unit_tests::checkThreadPoolPausing();
    return 0;
}