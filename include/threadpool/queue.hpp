#ifndef INCLUDE_GUARD_QUEUE_HPP
#define INCLUDE_GUARD_QUEUE_HPP

#include <deque>
#include <mutex>
#include <memory>
#include <utility>

#include <threadpool/task.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // TaskQueue class declaration
    ////////////////////////////////////////////////////////////////////////////////


    class TaskQueue : private std::deque<std::unique_ptr<Task>> {
    private:

        friend class ThreadPool;

        using value_t = std::unique_ptr<Task>;
        using deque_t = std::deque<value_t>;

    private:

        void PushBack(value_t&& task);
        void Clear() noexcept;
        bool Empty() const noexcept;
        void PopFront(value_t& task) noexcept;

    private:

        mutable std::mutex mtx_;

    };

}

#endif