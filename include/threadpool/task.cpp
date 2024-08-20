#include <threadpool/task.hpp>

#include <utility>
#include <stdexcept>
#include <type_traits>

namespace vsock {

    //////////////////////////////////////////////////////////////////////////////////
    // Task class defenition
    ////////////////////////////////////////////////////////////////////////////////

    Task::Task(Task&& other) :
        vars(std::move(other.vars)),
        type_{ std::exchange(other.type_,TaskType::ASYNC) },
        is_void_{ std::exchange(other.is_void_,true) },
        sync_task_{ std::exchange(other.sync_task_,{}) },
        async_task_{ std::exchange(other.async_task_,{}) },
        condition_{ std::exchange(other.condition_,{}) }
    {}

    Task& Task::operator=(Task&& other) {
        if (this != &other) {
            vars = std::move(other.vars);
            type_ = std::exchange(other.type_, TaskType::ASYNC);
            is_void_ = std::exchange(other.is_void_, true);
            sync_task_ = std::exchange(other.sync_task_, {});
            async_task_ = std::exchange(other.async_task_, {});
            condition_ = std::exchange(other.condition_, {});
        }
        return *this;
    }

    bool Task::IsVoidResult() {
        return is_void_;
    }

    bool Task::operator()() {
        switch (type_) {
            case TaskType::SYNC: {
                (*sync_task_)();
                return false;
            } break;
            case TaskType::LOOP: {
                if (!condition_.get() || !async_task_.get()) {
                    throw std::runtime_error("condition or loop is not set");
                }
                if ((*condition_)(*this)) {
                    (*async_task_)(*this);
                    return true;
                }
                return false;
            } break;
            default: { // TaskType::ASYNC
                (*async_task_)(*this);
                return false;
            }
        }
    }

}