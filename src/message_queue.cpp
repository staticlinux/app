module;

#include <array>
#include <atomic>
#include <errno.h>
#include <functional>
#include <future>
#include <memory>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

export module message_queue;
import cppl;
import log;

using cppl::task_state_t;
using cppl::task_t;

export class message_queue_t {
public:
    static message_queue_t& current()
    {
        static thread_local message_queue_t s_current {};
        return s_current;
    }

    message_queue_t()
    {
        // Create epoll fd.
        if ((m_epollfd = epoll_create1(/*flags=*/0)) < 0) {
            throw std::system_error { errno, std::system_category(), "create epoll failed" };
        }
    }

    ~message_queue_t()
    {
        if (m_epollfd) {
            close(m_epollfd);
        }
    }

    task_t<void> await(int fd, uint32_t events)
    {
        auto task_state = std::make_shared<task_state_t<void>>();
        auto evt_ctx = std::make_unique<event_context_t>(fd, task_state);
        auto evt = epoll_event {
            .events = events,
            .data = {
                .ptr = evt_ctx.get(),
            },
        };
        if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd, &evt) < 0) {
            throw std::system_error { errno, std::system_category(), "add fd to epoll failed" };
        }
        evt_ctx.release();
        return task_state;
    }

    template <typename T>
    T wait(task_t<T> task)
    {
        while (!task.await_ready()) {
            process_events();
        }
        return task.await_resume();
    }

private:
    struct event_context_t {
        int fd {};
        std::shared_ptr<task_state_t<void>> task_state {};
    };

    void process_events()
    {
        std::array<epoll_event, 256> events;
        auto numEvents = epoll_wait(m_epollfd, events.data(), events.size(), /*timeout=*/-1);
        if (numEvents < 0) {
            if (errno == EINTR) {
                // interrupted by signal, save to return.
                return;
            } else {
                throw std::system_error { errno, std::system_category(), "epoll_wait failed" };
            }
        }

        for (int i = 0; i < numEvents; ++i) {
            auto evt_ctx = reinterpret_cast<event_context_t*>(events[i].data.ptr);
            if (epoll_ctl(m_epollfd, EPOLL_CTL_DEL, evt_ctx->fd, nullptr) < 0) {
                throw std::system_error { errno, std::system_category(), "delete fd from epoll failed" };
            }
            (std::unique_ptr<event_context_t> { evt_ctx })->task_state->set_value();
        }
    }

    int m_epollfd {};
};