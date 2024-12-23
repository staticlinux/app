module;

#include <algorithm>
#include <coroutine>
#include <cstdint>
#include <errno.h>
#include <system_error>
#include <unistd.h>
#include <vector>

export module read_stream;
import message_queue;
import task;

task_t<std::vector<uint8_t>> read_async(int fd, size_t atMost)
{
    std::vector<uint8_t> buffer(atMost);
    while (true) {
        auto num = read(fd, buffer.data(), buffer.size());
        if (num == 0) {
            // socket has been closed.
            co_return {};
        } else if (num < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data, wait.
                co_await message_queue_t::current().await(fd, EPOLLIN);
            } else if (errno == EINTR) {
                // interrupted by signal, retry.
                continue;
            } else {
                throw std::system_error { errno, std::system_category(), "read failed" };
            }
        } else {
            buffer.resize(num);
            break;
        }
    }
    co_return buffer;
}

export class read_stream_t {
public:
    read_stream_t(int fd)
        : m_fd { fd }
    {
    }

    task_t<std::string> read_line_async()
    {
        while (true) {
            // Do we have '\n' in the buffer?
            auto it = std::find(m_buffer.begin(), m_buffer.end(), (uint8_t)'\n');
            if (it != m_buffer.end()) {
                auto line = std::string { m_buffer.begin(), it };

                // remove the '\r' if the last character is.
                if (!line.empty() && *line.rbegin() == '\r') {
                    line.pop_back();
                }

                // Resize the buffer.
                m_buffer = std::vector<uint8_t> { it + 1, m_buffer.end() };

                // All done.
                co_return line;
            }

            // no '\n' in the buffer, try to read more from the input.
            auto more = co_await ::read_async(m_fd, 1024);
            if (more.empty()) {
                // No more data.
                throw std::runtime_error { "connection is closed" };
            }

            // Attach to the buffer.
            m_buffer.insert(m_buffer.end(), more.begin(), more.end());
        }
    }

    task_t<std::vector<uint8_t>> read_async(size_t size)
    {
        if (m_buffer.size() >= size) {
            auto data = std::vector<uint8_t> { m_buffer.begin(), m_buffer.begin() + size };
            m_buffer = std::vector<uint8_t> { m_buffer.begin() + size, m_buffer.end() };
            std::swap(data, m_buffer);
            co_return data;
        } else {
            auto data = std::move(m_buffer);
            auto remain = size - data.size();
            while (remain) {
                auto more = co_await ::read_async(m_fd, remain);
                if (more.empty()) {
                    // No more data.
                    throw std::runtime_error { "connection is closed" };
                }
                data.insert(data.end(), more.begin(), more.end());
                remain -= more.size();
            }
            co_return data;
        }
    }

private:
    int m_fd {};
    std::vector<uint8_t> m_buffer {};
};