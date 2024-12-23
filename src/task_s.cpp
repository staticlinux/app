module;

#include <cassert>
#include <coroutine>
#include <future>

export module task;

export template <typename T>
class task_state_t {
public:
    bool is_ready() const
    {
        using namespace std::chrono_literals;

        return m_future.wait_for(0s) == std::future_status::ready;
    }

    void set_handle(std::coroutine_handle<> h)
    {
        m_handle = std::move(h);
    }

    void set_value(T value)
    {
        m_promise.set_value(std::move(value));
        if (m_handle) {
            m_handle();
        }
    }

    void set_exception(std::exception_ptr ptr)
    {
        m_promise.set_exception(std::move(ptr));
        if (this->m_handle) {
            this->m_handle();
        }
    }

    T get_value()
    {
        return m_future.get();
    }

private:
    std::promise<T> m_promise {};
    std::future<T> m_future { m_promise.get_future() };
    std::coroutine_handle<> m_handle {};
};

export template <>
class task_state_t<void> {
public:
    bool is_ready() const
    {
        using namespace std::chrono_literals;

        return m_future.wait_for(0s) == std::future_status::ready;
    }

    void set_handle(std::coroutine_handle<> h)
    {
        m_handle = std::move(h);
    }

    void set_value()
    {
        m_promise.set_value();
        if (m_handle) {
            m_handle();
        }
    }

    void set_exception(std::exception_ptr ptr)
    {
        m_promise.set_exception(std::move(ptr));
        if (this->m_handle) {
            this->m_handle();
        }
    }

    void get_value()
    {
        m_future.get();
    }

private:
    std::promise<void> m_promise {};
    std::future<void> m_future { m_promise.get_future() };
    std::coroutine_handle<> m_handle {};
};

export template <typename T>
struct task_t {
    struct promise_type {
        task_t<T> get_return_object()
        {
            return m_state;
        }

        std::suspend_never initial_suspend()
        {
            return {};
        }

        std::suspend_never final_suspend() noexcept
        {
            return {};
        }

        void unhandled_exception()
        {
            m_state->set_exception(std::current_exception());
        }

        void return_value(T value)
        {
            m_state->set_value(std::move(value));
        }

    private:
        std::shared_ptr<task_state_t<T>> m_state { std::make_shared<task_state_t<T>>() };
    };

    task_t(std::shared_ptr<task_state_t<T>> state)
        : m_state { std::move(state) }
    {
    }

    bool await_ready() const
    {
        return m_state->is_ready();
    }

    void await_suspend(std::coroutine_handle<> h)
    {
        m_state->set_handle(std::move(h));
    }

    T await_resume()
    {
        return m_state->get_value();
    }

private:
    std::shared_ptr<task_state_t<T>> m_state {};
};

export template <>
struct task_t<void> {
    struct promise_type {
        task_t<void> get_return_object()
        {
            return m_state;
        }

        std::suspend_never initial_suspend()
        {
            return {};
        }

        std::suspend_never final_suspend() noexcept
        {
            return {};
        }

        void unhandled_exception()
        {
            m_state->set_exception(std::current_exception());
        }

        void return_void()
        {
            m_state->set_value();
        }

    private:
        std::shared_ptr<task_state_t<void>> m_state { std::make_shared<task_state_t<void>>() };
    };

    task_t(std::shared_ptr<task_state_t<void>> state)
        : m_state { std::move(state) }
    {
        assert(m_state);
    }

    bool await_ready() const
    {
        return m_state->is_ready();
    }

    void await_suspend(std::coroutine_handle<> h)
    {
        m_state->set_handle(std::move(h));
    }

    void await_resume()
    {
        m_state->get_value();
    }

private:
    std::shared_ptr<task_state_t<void>> m_state {};
};