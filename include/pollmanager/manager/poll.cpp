#include <pollmanager/manager/poll.hpp>
#include <core/error.hpp>
#include <algorithm>
#include <iostream>

using namespace std;

namespace vsock {

    PollManager::PollManager(ThreadPool* const thread_pool) :
        epollfd_{ NULL },
        thread_pool_{ thread_pool },
        epoll_result_{ nullptr },
        is_alive_{ false },
        poll_running_{ false },
        is_stoping_{ false },
        abort_event_fd_{ 0 }
    {
        CreateEpoll_();
    }

    PollManager::~PollManager() {
        Stop_();
        DestroyEpoll_();
    }

    void PollManager::Add(
        const SocketID socket_id,
        const std::uint32_t flags,
        callback_func_t&& callback
    ) {
        if (is_stoping_) {
            return;
        }
        {
            std::scoped_lock queue_lock(queue_mtx_);

            auto res = queue_.insert(
                {
                    socket_id,
                    {
                        flags,
                        std::forward<callback_func_t>(callback)
                    }
                }
            );

            if (res.second == false) {
                return;
            }

        }

        struct epoll_event ev;
        ev.events = flags;
        ev.data.fd = socket_id;

        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, socket_id, &ev) == -1) {
            throw RuntimeError(
                "Method: PollManager::Add()"s,
                "Message: ::epoll_ctl() failed"s
            );
        }


        if (!is_alive_) {
            Start_();
        }
        data_cv_.notify_all();

    }

    void PollManager::Remove(const SocketID socket_id) {
        if (!is_alive_ || is_stoping_) {
            return;
        }

        {
            std::scoped_lock queue_lock(queue_mtx_);

            if (queue_.find(socket_id) == queue_.end()) {
                return;
            }

            if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, socket_id, NULL) == -1) {
                throw RuntimeError(
                    "Method: PollManager::Remove()"s,
                    "Message: ::epoll_ctl() failed"s
                );
            }

            queue_.erase(socket_id);

        }

    }

    void PollManager::Start_() {
        is_alive_ = true;

        (*thread_pool_).AddAsyncTask([this]() {
            poll_running_ = true;
            Poll_();
            std::unique_lock stop_cv_lock(stop_cv_mtx_);
            poll_running_ = false;
            stop_cv_.notify_one();
        });

    }

    void PollManager::Stop_() {
        is_stoping_ = true;
        is_alive_ = false;
        data_cv_.notify_all();
        SendAbortSignal_();

        std::unique_lock stop_cv_lock(stop_cv_mtx_);
        while (poll_running_) {
            stop_cv_.wait(stop_cv_lock);
        }
        stop_cv_lock.unlock();

        ClearPollsAndQueue_();
    }

    void PollManager::Poll_() {
        std::unique_lock data_cv_lock(data_cv_mtx_);
        while (is_alive_) {
            while (queue_.empty() && is_alive_ && !is_stoping_) {
                data_cv_.wait(data_cv_lock);
            }

            if (!is_alive_ || is_stoping_) {
                return;
            }

            int nfds = epoll_wait(epollfd_, epoll_result_, VSOCK_EPOLL_MAX_EVENTS, VSOCK_EPOLL_TIMEOUT);

            if (!is_alive_ || is_stoping_) {
                return;
            }

            if (nfds == -1) {
                throw RuntimeError(
                    "Method: PollManager::Poll_()"s,
                    "Message: ::epoll_wait() failed"s
                );
            }
            else if (nfds == 0) {
                continue;
            }
            else {
                for (int n = 0; n < nfds; ++n) {

                    SocketID socket_id = epoll_result_[n].data.fd;

                    (*thread_pool_).AddAsyncTask([this, id = socket_id]() {
                        callback_func_t callback;
                        bool found = false;
                        {
                            std::scoped_lock queue_lock(queue_mtx_);
                            if (queue_.find(id) != queue_.end()) {
                                found = true;
                                callback = queue_.at(id).callback;
                            }
                        }
                        if (found) {
                            callback(id);
                        }
                    });

                }
            }

        }
    }

    void PollManager::ResetFlags(const SocketID socket_id) {
        if (!is_alive_ || is_stoping_) {
            return;
        }
        {
            std::scoped_lock queue_lock(queue_mtx_);

            if (queue_.find(socket_id) == queue_.end()) {
                return;
            }

            struct epoll_event ev;
            ev.events = queue_.at(socket_id).flags;
            ev.data.fd = socket_id;
            if (epoll_ctl(epollfd_, EPOLL_CTL_MOD, socket_id, &ev) == -1) {
                throw RuntimeError(
                    "Method: PollManager::ResetFlags_()"s,
                    "Message: ::epoll_ctl() failed"s
                );
            }
        }
    }

    void PollManager::CreateEpoll_() {

        epoll_result_ = new struct epoll_event[VSOCK_EPOLL_MAX_EVENTS];

        epollfd_ = ::epoll_create1(0);
        if (epollfd_ == VSOCK_EPOLL_ERROR) {
            throw RuntimeError(
                "Method: PollManager::CreateEpoll_()"s,
                "Message: ::epoll_create1() failed"s
            );
        }

        CreateAbortEvent_();

    }

    void PollManager::DestroyEpoll_() {

        DestroyAbortEvent_();

        #ifdef _WIN32
        if (::epoll_close(epollfd_) == -1) {
            throw RuntimeError(
                "Method: PollManager::DestroyEpoll_()"s,
                "Message: ::epoll_close() failed"s
            );
        }
        epollfd_ = NULL;
        #else
        if (::close(epollfd_) == VSOCK_EPOLL_ERROR) {
            throw RuntimeError(
                "Method: PollManager::DestroyEpoll_()"s,
                "Message: ::close() failed"s
            );
        }
        epollfd_ = -1;
        #endif

        delete[] epoll_result_;

    }

    void PollManager::CreateAbortEvent_() {
        #ifndef _WIN32
        abort_event_fd_ = eventfd(0, 0);
        if (abort_event_fd_ == VSOCK_EPOLL_ERROR) {
            throw RuntimeError(
                "Method: PollManager::CreateAbortEvent_()"s,
                "Message: eventfd() failed"s
            );
        }
        struct epoll_event ev;
        ev.events = (EPOLLIN | EPOLLONESHOT);
        ev.data.fd = abort_event_fd_;
        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, abort_event_fd_, &ev) == -1) {
            throw RuntimeError(
                "Method: PollManager::CreateAbortEvent_()"s,
                "Message: ::epoll_ctl() failed"s
            );
        }
        #endif
    }

    void PollManager::DestroyAbortEvent_() {
        #ifndef _WIN32
        if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, abort_event_fd_, NULL) == VSOCK_EPOLL_ERROR) {
            throw RuntimeError(
                "Method: PollManager::DestroyAbortEvent_()"s,
                "Message: remove of abort_event_fd_ failed"s
            );
        }
        close(abort_event_fd_);
        #endif
    }

    void PollManager::SendAbortSignal_() {
        #ifdef _WIN32
        PostQueuedCompletionStatus(epollfd_, 0, 0, NULL);
        #else
        std::uint64_t one = 1;
        if (::write(abort_event_fd_, &one, sizeof(std::uint64_t)) != sizeof(std::uint64_t)) {
            throw RuntimeError(
                "Method: PollManager::SendAbortSignal_()"s,
                "Message: ::write() failed"s
            );
        }
        #endif        
        }

    void PollManager::ClearPollsAndQueue_() {
        for (const auto& [id,value] : queue_) {
            if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, id, NULL) == -1) {
                throw RuntimeError(
                    "Method: PollManager::ClearPollsAndQueue_()"s,
                    "Message: ::epoll_ctl() failed"s
                );
            }
            closesocket(id);
        }
        queue_.clear();
    }

    }

