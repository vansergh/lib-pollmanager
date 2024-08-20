#include <threadpool/threadpool.hpp>
#include <core/common.hpp>

#include <cstdint>
#include <map>
#include <functional>
#include <condition_variable>
#include <mutex>

namespace vsock {

    class PollManager {
    public:

        PollManager() = delete;
        PollManager(const PollManager&) = delete;
        PollManager(PollManager&&) = delete;
        PollManager& operator=(const PollManager&) = delete;
        PollManager& operator=(PollManager&&) = delete;

    private:

        typedef std::function<void(const SocketID)> callback_func_t;

        typedef struct {
            std::uint32_t flags;
            callback_func_t callback;
        } queue_record_t;

    public:

        PollManager(ThreadPool* const thread_pool);
        ~PollManager();

        void Add(
            const SocketID socket_id,
            const std::uint32_t flags,
            callback_func_t&& callback
        );
        void Remove(const SocketID socket_id);
        void ResetFlags(const SocketID socket_id);

    private:

        void Start_();
        void Stop_();
        void Poll_();

        

        void CreateEpoll_();
        void DestroyEpoll_();
        void CreateAbortEvent_();
        void DestroyAbortEvent_();
        void SendAbortSignal_();
        void ClearPollsAndQueue_();

    private:

        EpollID epollfd_;
        ThreadPool* const thread_pool_;
        struct epoll_event* epoll_result_;

        std::map<SocketID, queue_record_t> queue_;

        bool is_alive_;
        bool poll_running_;
        bool is_stoping_;

        int abort_event_fd_;

        std::mutex queue_mtx_;
        std::mutex data_cv_mtx_;
        std::mutex stop_cv_mtx_;
        std::condition_variable data_cv_;
        std::condition_variable stop_cv_;

    };

}