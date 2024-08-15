#ifndef INCLUDE_GUARD_THREADPOOL_HPP
#define INCLUDE_GUARD_THREADPOOL_HPP

#include <cstdint>
#include <cstddef> 
#include <thread>
#include <exception> 
#include <utility>
#include <mutex>
#include <memory>
#include <deque>
#include <future>
#include <functional>
#include <type_traits>
#include <condition_variable>

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
        ThreadPool(const std::uint32_t concurency);
        ThreadPool(const std::uint32_t concurency, const DestroyType destroy_type);
        ~ThreadPool();

        void Wait();
        void Pause();
        void Continue();
        void ClearTasks();
        void Reset();
        void Reset(const DestroyType destroy_type);
        void Reset(const std::uint32_t concurency);
        void Reset(const std::uint32_t concurency, const DestroyType destroy_type);

        template <typename FuncType>
        void AddAsyncTask(FuncType&& task);

        template <typename FuncType, typename RetType = std::invoke_result_t<std::decay_t<FuncType>>>
        [[nodiscard]] std::future<RetType> AddSyncTask(FuncType&& task);


        template <class FuncType, class... Args>
        auto AddTask(FuncType&& task_func, Args&&... args) -> std::future<decltype(task_func(args...))>;

    private:

        DestroyType destroy_type_;

        std::unique_ptr<std::thread[]> threads_;
        std::deque<std::function<void()>> tasks_;

        std::uint32_t threads_count_;
        std::uint64_t tasks_running_;

        bool working_;
        bool paused_;
        bool waiting_;

        mutable std::mutex tasks_mutex_;

        std::condition_variable tasks_available_cv_;
        std::condition_variable tasks_done_cv_;

        [[nodiscard]] std::uint32_t ChooseThreadsCount_(const std::uint32_t threads_count) const noexcept;
        void CreateThreads_();
        void StopThreads_();
        void DestroyThreads_();
        void Finish_();
        void Process_(const std::uint32_t thread_index);

    };

    //////////////////////////////////////////////////////////////////////////////////
    // ThreadPool class defenition (template methods)
    ////////////////////////////////////////////////////////////////////////////////

    template<typename FuncType>
    inline void ThreadPool::AddAsyncTask(FuncType&& task) {
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_.emplace_back(std::forward<FuncType>(task));
        }
        tasks_available_cv_.notify_one();
    }

    template<typename FuncType, typename RetType>
    inline std::future<RetType> ThreadPool::AddSyncTask(FuncType&& task) {
        const std::shared_ptr<std::promise<RetType>> task_promise = std::make_shared<std::promise<RetType>>();
        AddAsyncTask(
            [task = std::forward<FuncType>(task), task_promise] {
            try {
                if constexpr (std::is_void_v<RetType>) {
                    task();
                    task_promise->set_value();
                }
                else {
                    task_promise->set_value(task());
                }
            }
            catch (...) {
                try {
                    task_promise->set_exception(std::current_exception());
                }
                catch (...) {
                }
            }
        });
        return task_promise->get_future();
    }

    template<class FuncType, class ...Args>
    inline auto ThreadPool::AddTask(FuncType&& task_func, Args && ...args) -> std::future<decltype(task_func(args ...))> {

        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            if (!working_)
                throw std::runtime_error(
                    "Delegating task to a threadpool "
                    "that has been terminated or canceled.");
        }

        using return_t = decltype(task_func(args...));
        using future_t = std::future<return_t>;
        using task_t = std::packaged_task<return_t()>;

        auto bind_func = std::bind(std::forward<FuncType>(task_func), std::forward<Args>(args)...);
        std::shared_ptr<task_t> task = std::make_shared<task_t>(std::move(bind_func));
        future_t res = task->get_future();
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_.emplace_back([task]() -> void {
                (*task)();
            });
        }

        tasks_available_cv_.notify_one();
        return res;
    }

}

#endif // INCLUDE_GUARD_THREADPOOL_HPP
