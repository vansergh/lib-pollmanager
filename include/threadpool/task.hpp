#ifndef INCLUDE_GUARD_TASK_HPP
#define INCLUDE_GUARD_TASK_HPP

#include <cstdint>
#include <cstddef>
#include <functional>
#include <future>

#include <threadpool/varlist.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // Task class declaration
    ////////////////////////////////////////////////////////////////////////////////

    class Task {
    private:
        enum class TaskType : std::uint8_t { ASYNC, SYNC, LOOP };
    public:

        Task() = default;

        Task(Task&& other);
        Task& operator=(Task&& other);

        template<typename F, typename... Args>
        auto SetSyncJob(F&& job, Args&&... args);

        template<typename F, typename... Args>
        void SetAsyncJob(F&& loop, Args&&... args);

        template<typename F, typename... Args>
        void SetLoopJob(F&& loop, Args&&... args);

        template<typename F, typename... Args>
        void SetCondition(F&& condition, Args&&... args);

        bool IsVoidResult();

        bool operator()();

    public:

        VarList vars;

    private:

        TaskType type_{ TaskType::ASYNC };
        bool is_void_{ true };
        std::unique_ptr<std::packaged_task<void(void)>> sync_task_;
        std::unique_ptr<std::function<void(Task&)>> async_task_{ nullptr };
        std::unique_ptr<std::function<bool(Task&)>> condition_{ nullptr };

    };

    //////////////////////////////////////////////////////////////////////////////////
    // ThreadPool class defenition (template methods)
    ////////////////////////////////////////////////////////////////////////////////

    template<typename F, typename ...Args>
    inline auto Task::SetSyncJob(F&& job, Args && ...args) {

        type_ = TaskType::SYNC;
        condition_.reset();
        async_task_.reset();

        using return_type = std::invoke_result_t<F, Args...>;
        using promise_type = std::promise<return_type>;
        using bind_type = std::function<return_type(void)>;

        is_void_ = std::is_void_v<return_type>;

        const std::shared_ptr<bind_type> bind_fnc_ptr = std::make_shared<bind_type>(std::bind(std::forward<F>(job), std::forward<Args>(args)...));
        const std::shared_ptr<promise_type> task_promise_ptr = std::make_shared<promise_type>();
        sync_task_.reset();
        sync_task_ = std::make_unique<std::packaged_task<void(void)>>(
            std::packaged_task([bind_fnc_ptr, task_promise_ptr]() {
            try {
                if constexpr (std::is_void_v<return_type>) {
                    (*bind_fnc_ptr)();
                    task_promise_ptr->set_value();
                }
                else {
                    task_promise_ptr->set_value((*bind_fnc_ptr)());
                }
            }
            catch (...) {
                try {
                    task_promise_ptr->set_exception(std::current_exception());
                }
                catch (...) {
                    throw std::runtime_error("set_exception() failed");
                }
            }
        }));


        return task_promise_ptr->get_future();
    }

    template<typename F, typename ...Args>
    inline void Task::SetAsyncJob(F&& job, Args && ...args) {
        type_ = TaskType::ASYNC;
        sync_task_.reset();
        condition_.reset();
        is_void_ = true;
        async_task_.reset();
        async_task_ = std::make_unique<std::function<void(Task&)>>(
            std::bind(std::forward<F>(job), std::forward<Args>(args)...)
        );
    }

    template<typename F, typename ...Args>
    inline void Task::SetCondition(F&& condition, Args && ...args) {
        type_ = TaskType::LOOP;
        is_void_ = true;
        sync_task_.reset();
        condition_.reset();
        condition_ = std::make_unique<std::function<bool(Task&)>>(
            std::bind(std::forward<F>(condition), std::forward<Args>(args)...)
        );
    }

    template<typename F, typename ...Args>
    inline void Task::SetLoopJob(F&& loop, Args && ...args) {
        type_ = TaskType::LOOP;
        is_void_ = true;
        sync_task_.reset();
        async_task_.reset();
        async_task_ = std::make_unique<std::function<void(Task&)>>(
            std::bind(std::forward<F>(loop), std::forward<Args>(args)...)
        );
    }

}

#endif // INCLUDE_GUARD_TASK_HPP