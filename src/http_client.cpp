module;

#include <algorithm>
#include <coroutine>
#include <cstddef>
#include <format>
#include <future>
#include <netdb.h>
#include <netinet/in.h>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <system_error>
#include <unordered_map>
#include <vector>

export module http_client;
import cppl;
import message_queue;
import read_stream;
import string_utils;

using cppl::task_state_t;
using cppl::task_t;

struct uri_view_t {
    std::string schema;
    std::string host;
    uint16_t port;
    std::string_view path;
};

uint16_t get_schema_default_port(std::string_view schema)
{
    if (schema == "http") {
        return 80;
    }
    throw new std::runtime_error { std::format("unknown default port for schema: {}", schema) };
}

uri_view_t parse_uri(std::string_view str)
{
    uri_view_t uri {};

    auto pos = str.find("://");
    if (pos == std::string::npos) {
        throw std::runtime_error { "Invalid Uri" };
    } else {
        uri.schema = tolower(str.substr(0, pos));
    }

    std::string hostAndPort;
    std::string path;
    auto pathStart = str.find('/', pos + 3);
    if (pathStart == std::string::npos) {
        hostAndPort = str.substr(pos + 3);
        uri.path = "/";
    } else {
        hostAndPort = str.substr(pos + 3, pathStart - pos - 3);
        uri.path = str.substr(pathStart);
    }

    auto portStart = hostAndPort.find(':');
    if (portStart == std::string::npos) {
        uri.host = hostAndPort;
        uri.port = get_schema_default_port(uri.schema);
    } else {
        uri.host = hostAndPort.substr(0, portStart);
        uri.port = atoi(hostAndPort.substr(portStart + 1).c_str());
    }
    if (uri.port <= 0 || uri.port > 65535) {
        throw std::runtime_error { "Invalid whipEndpoint" };
    }

    return uri;
}

task_t<void> write_async(int fd, std::string_view data)
{
    auto reamin = data.size();
    auto p = data.data();
    while (reamin) {
        auto num = write(fd, p, reamin);
        if (num == 0) {
            // socket has been closed.
            throw std::runtime_error { "connection has been closed by remote" };
        } else if (num < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // can't write more data, wait.
                co_await message_queue_t::current().await(fd, EPOLLOUT);
            } else if (errno == EINTR) {
                // interrupted by signal, retry.
                continue;
            } else {
                throw std::system_error { errno, std::system_category(), "write failed" };
            }
        }
        reamin -= num;
    }
}

task_t<int> http_read_status_line_async(read_stream_t& stream)
{
    auto status = 0;
    auto line = co_await stream.read_line_async();
    auto it = line.find(' ');
    if (it == std::string::npos || !(status = atoi(line.c_str() + it + 1))) {
        throw std::runtime_error { std::format("Bad http status line: {}", line) };
    }
    co_return status;
}

task_t<std::unordered_multimap<std::string, std::string>> http_read_headers_async(read_stream_t& stream)
{
    std::unordered_multimap<std::string, std::string> header;
    while (true) {
        auto line = co_await stream.read_line_async();
        if (line.empty()) {
            break;
        }

        // find ':'
        auto it = line.find(':');
        if (it == std::string::npos) {
            throw std::runtime_error { std::format("Invalid http header: {}", line) };
        }

        auto key = tolower(trim(line.substr(0, it)));
        if (key.empty()) {
            throw std::runtime_error { std::format("Invalid http header: {}", line) };
        }

        auto value = trim(line.substr(it + 1));
        header.emplace(std::move(key), std::move(value));
    }
    co_return header;
}

read_stream_t http_open(const std::string& host, uint16_t port)
{
    // create socket.
    auto sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd < 0) {
        throw std::system_error { errno, std::system_category(), "create socket failed" };
    }

    auto read_stream = read_stream_t { sd };

    // reuse addr.
    int yes = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0) {
        throw std::system_error { errno, std::system_category(), "reuse addr failed" };
    }

    // set non-blocking
    int on = 1;
    if (int ret = ioctl(sd, FIONBIO, (char*)&on); ret < 0) {
        throw std::system_error { errno, std::system_category(), "ioctl() set to non-blocking failed." };
    }

    // Fill remote address.
    sockaddr_in addr {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    if (auto hent = gethostbyname(host.c_str()); !hent) {
        throw std::system_error { h_errno, std::system_category(), "gethostbyname failed" };
    } else if (auto pAddrList = (in_addr**)hent->h_addr_list; *pAddrList) {
        addr.sin_addr = **pAddrList;
    } else {
        throw std::runtime_error { "gethostbyname return empty" };
    }

    // Connect.
    auto ret = connect(sd, (const sockaddr*)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        throw std::system_error { errno, std::system_category(), "connect to failed" };
    }

    return read_stream;
}

export task_t<std::pair<std::unordered_multimap<std::string, std::string>, read_stream_t>> http_get_header_async(std::string_view url, const std::unordered_multimap<std::string, std::string>& headers)
{
    auto uri = parse_uri(url);
    if (uri.schema != "http") {
        throw std::runtime_error { "only support http" };
    }

    auto read_stream = http_open(uri.host, uri.port);

    std::string request_headers = std::format("GET {} HTTP/1.1\r\n"
                                              "Host: {}\r\n"
                                              "User-Agent: staticlinux.org/app\r\n"
                                              "Accept: */*\r\n",
        uri.path, uri.host);

    // Write customer headers.
    for (const auto& header : headers) {
        request_headers += std::format("{}: {}\r\n", header.first, header.second);
    }
    request_headers += "\r\n";

    // Write.
    co_await write_async(read_stream.native_handle(), request_headers);

    // Header end.
    co_await write_async(read_stream.native_handle(), "\r\n");

    // Read status code.
    auto status = co_await http_read_status_line_async(read_stream);
    if (status < 200 || status > 299) {
        throw std::runtime_error { std::format("server return error: {}", status) };
    }

    // Read headers.
    auto response_headers = co_await http_read_headers_async(read_stream);
    co_return std::make_pair(std::move(response_headers), std::move(read_stream));
}

export task_t<std::pair<std::unordered_multimap<std::string, std::string>, read_stream_t>> http_get_header_async(std::string_view url)
{
    return http_get_header_async(url, /*headers=*/ {});
}

export task_t<std::vector<uint8_t>> http_get_async(std::string_view url, const std::unordered_multimap<std::string, std::string>& headers)
{
    auto [response_headers, read_stream] = co_await http_get_header_async(url, headers);

    // Get content-length.
    auto it = response_headers.find("content-length");
    if (it == response_headers.end()) {
        throw std::runtime_error { std::format("no content-length header") };
    }
    auto content_length = atoi(it->second.c_str());
    if (content_length <= 0 && it->second != "0") {
        throw std::runtime_error { std::format("invalid content-length.") };
    }

    co_return co_await read_stream.read_async(content_length);
}

export task_t<std::vector<uint8_t>> http_get_async(std::string_view url)
{
    return http_get_async(url, {});
}