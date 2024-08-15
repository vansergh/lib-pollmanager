#include <threadpool/threadpool.hpp>

namespace vsock {

    ThreadPool::ThreadPool(const std::uint32_t concurency, const DestroyType destroy_type) :
        destroy_type_{ destroy_type },
        threads_{ std::make_unique<std::thread[]>(ChooseThreadsCount_(concurency)) },
        tasks_{ },
        threads_count_{ ChooseThreadsCount_(concurency) },
        tasks_running_{ 0 },
        working_{ false },
        paused_{ false },
        waiting_{ false }
    {
        CreateThreads_();
    }

    ThreadPool::ThreadPool() :
        ThreadPool(std::thread::hardware_concurrency(), DestroyType::SMOOTH)
    {}

    ThreadPool::ThreadPool(const DestroyType destroy_type) :
        ThreadPool(std::thread::hardware_concurrency(), destroy_type)
    {}

    ThreadPool::ThreadPool(const std::uint32_t concurency) :
        ThreadPool(concurency, DestroyType::SMOOTH)
    {}

    ThreadPool::~ThreadPool() {
        Finish_();
    }

    void ThreadPool::ClearTasks() {
        const std::scoped_lock tasks_lock(tasks_mutex_);
        tasks_.clear();
    }

    void ThreadPool::Reset() {
        Reset(std::thread::hardware_concurrency(), DestroyType::SMOOTH);
    }

    void ThreadPool::Reset(const DestroyType destroy_type) {
        Reset(std::thread::hardware_concurrency(), destroy_type);
    }

    void ThreadPool::Reset(const std::uint32_t concurency) {
        Reset(concurency, DestroyType::SMOOTH);
    }

    void ThreadPool::Reset(const std::uint32_t concurency, const DestroyType destroy_type) {
        std::unique_lock tasks_lock(tasks_mutex_);
        destroy_type_ = destroy_type;
        const bool was_paused = paused_;
        paused_ = true;
        tasks_lock.unlock();
        Finish_();
        threads_count_ = ChooseThreadsCount_(concurency);
        threads_ = std::make_unique<std::thread[]>(threads_count_);
        CreateThreads_();
        tasks_lock.lock();
        paused_ = was_paused;
    }

    void ThreadPool::Wait() {
        std::unique_lock tasks_lock(tasks_mutex_);
        waiting_ = true;
        tasks_done_cv_.wait(
            tasks_lock,
            [this] {return (tasks_running_ == 0) && (paused_ || tasks_.empty());}
        );
        waiting_ = false;
    }

    void ThreadPool::Pause() {
        const std::scoped_lock tasks_lock(tasks_mutex_);
        paused_ = true;
    }

    void ThreadPool::Continue() {
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            paused_ = false;
        }
        tasks_available_cv_.notify_all();
    }

    std::uint32_t ThreadPool::ChooseThreadsCount_(const std::uint32_t threads_count) const noexcept {
        if (threads_count > 0) {
            return threads_count;
        }
        if (std::thread::hardware_concurrency() > 0) {
            return std::thread::hardware_concurrency();
        }
        return 1;
    }

    void ThreadPool::CreateThreads_() {

        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            tasks_running_ = threads_count_;
            working_ = true;
        }

        for (std::uint32_t index = 0; index < threads_count_; ++index) {
            threads_[index] = std::thread(&ThreadPool::Process_, this, index);
        }

    }

    void ThreadPool::StopThreads_() {
        {
            const std::scoped_lock tasks_lock(tasks_mutex_);
            working_ = false;
        }
        tasks_available_cv_.notify_all();
        for (std::uint32_t i = 0; i < threads_count_; ++i) {
            threads_[i].join();
        }
    }

    void ThreadPool::DestroyThreads_() {
        ClearTasks();
        StopThreads_();
    }

    void ThreadPool::Finish_() {
        if (destroy_type_ == DestroyType::SHARP) {
            DestroyThreads_();
        }
        else {
            Wait();
            StopThreads_();
        }
    }

    void ThreadPool::Process_([[maybe_unused]] const std::uint32_t thread_index) {
        std::unique_lock tasks_lock(tasks_mutex_);
        while (true) {
            --tasks_running_;
            tasks_lock.unlock();
            if (waiting_ && tasks_running_ == 0 && (paused_ || tasks_.empty())) {
                tasks_done_cv_.notify_all();
            }
            tasks_lock.lock();
            tasks_available_cv_.wait(tasks_lock,
                [this] {
                return !(paused_ || tasks_.empty()) || !working_;
            });
            if (!working_)
                break;
            {
                const std::function<void()> task = std::move(tasks_.front());
                tasks_.pop_front();
                ++tasks_running_;
                tasks_lock.unlock();
                task();
            }
            tasks_lock.lock();
        }
    }

}