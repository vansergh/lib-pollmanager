#ifndef INCLUDE_GUARD_THREADPOOL_HPP
#define INCLUDE_GUARD_THREADPOOL_HPP

#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>

#include <threadpool/task.hpp>
#include <threadpool/queue.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // ThreadPool class declaration
    ////////////////////////////////////////////////////////////////////////////////

    class ThreadPool {
    public:

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

    public:

        enum class DestroyType : std::uint8_t {
            SMOOTH,
            SHARP
        };

        ThreadPool();
        ThreadPool(const DestroyType destroy_type);
        ThreadPool(const std::size_t concurency);
        ThreadPool(const std::size_t concurency, const DestroyType destroy_type);
        ~ThreadPool();

        void Wait() noexcept;
        void Pause() noexcept;
        void Continue() noexcept;
        void ClearTasks() noexcept;

        void Reset();
        void Reset(const DestroyType destroy_type);
        void Reset(const std::size_t concurency);
        void Reset(const std::size_t concurency, const DestroyType destroy_type);

        void AddSyncTask(std::unique_ptr<Task> task);
        void AddAsyncTask(std::unique_ptr<Task> task);

        template<typename F, typename...Args>
        auto AddSyncTask(F&& job, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

        template<typename F, typename...Args>
        void AddAsyncTask(F&& job, Args&&... args);

    private:

        DestroyType destroy_type_;

        std::unique_ptr<std::thread[]> threads_;
        TaskQueue tasks_;

        std::size_t threads_count_;
        std::size_t tasks_running_;

        bool working_;
        bool paused_;
        bool waiting_;

        std::mutex tasks_mutex_;

        std::condition_variable tasks_available_cv_;
        std::condition_variable tasks_done_cv_;

        [[nodiscard]] std::size_t ChooseThreadsCount_(const std::size_t threads_count) const noexcept;
        void CreateThreads_();
        void StopThreads_();
        void DestroyThreads_();
        void Finish_();
        void Process_();

    };

    //////////////////////////////////////////////////////////////////////////////////
    // ThreadPool class defenition (template methods)
    ////////////////////////////////////////////////////////////////////////////////

    template<typename F, typename...Args>
    auto ThreadPool::AddSyncTask(F&& job, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        std::unique_ptr<Task> task_ptr(std::make_unique<Task>());
        auto result = task_ptr->SetSyncJob(std::forward<F>(job), std::forward<Args>(args)...);
        tasks_.PushBack(std::move(task_ptr));
        tasks_available_cv_.notify_one();
        return result;
    }

    template<typename F, typename...Args>
    void ThreadPool::AddAsyncTask(F&& job, Args&&... args) {
        std::unique_ptr<Task> task_ptr(std::make_unique<Task>());
        task_ptr->SetAsyncJob(std::forward<F>(job), std::forward<Args>(args)...);
        tasks_.PushBack(std::move(task_ptr));
        tasks_available_cv_.notify_one();
    }

}

#endif // INCLUDE_GUARD_THREADPOOL_HPP
