#include <threadpool/queue.hpp>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // TaskQueue class defenition
    ////////////////////////////////////////////////////////////////////////////////    

    void TaskQueue::PushBack(value_t&& task) {
        const std::scoped_lock rw_lock(mtx_);
        std::deque<std::unique_ptr<Task>>::push_back(std::move(task));
    }

    void TaskQueue::Clear() noexcept {
        const std::scoped_lock rw_lock(mtx_);
        std::deque<std::unique_ptr<Task>>::clear();
    }

    bool TaskQueue::Empty() const noexcept {
        const std::scoped_lock rw_lock(mtx_);
        return std::deque<std::unique_ptr<Task>>::empty();
    }

    void TaskQueue::PopFront(value_t& task) noexcept {
        const std::scoped_lock rw_lock(mtx_);
        task = std::move(std::deque<std::unique_ptr<Task>>::front());
        std::deque<std::unique_ptr<Task>>::pop_front();
    }

}