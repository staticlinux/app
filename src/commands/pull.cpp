module;

#include <cassert>
#include <coroutine>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>

import consts;
import http_client;
import log;
import task;

export module pull;

struct Options {
    bool help {};
};

static Options parse_options(int& argc, const char**& argv)
{
    auto options = Options {};
    while (argc && **argv == '-') {
        if (!strcmp(*argv + 1, "h") || !strcmp(*argv + 1, "-help")) {
            options.help = true;
            --argc;
            ++argv;
        } else {
            fatal_error("unknown option: {}", *argv);
        }
    }
    return options;
}

static void print_help()
{
    fprintf(stdout, R"(Download app from staticlinux.org
Usage: app pull [OPTIONS] NAME/PATH:VERSION

Options:
    -h,--help                   Print this help message and exit

Parameters:
    NAME                        Name of the package
    PATH                        App file path under the package
    VERSION                     Version of the package

For more information, please visit %s/commands/pull
)",
        DOC_BASE_LINK);
}

static task_t<void> pull_async(std::string name, std::string version, std::string filePath)
{
    assert(!name.empty());
    assert(!version.empty());
    assert(!filePath.empty());

    auto arch = sizeof(void*) == 4 ? "x86" : "amd64";
    auto downloadPath = std::format("{}/{}/{}/{}/{}", APP_DOWNLOAD_BASE_LINK, name, version, arch, filePath);
    status("Pulling from {}", downloadPath);

    auto data = co_await http_get_async(downloadPath);
    status("Pull completed");
    status("Size: {}", (int)data.size());
    status("(TODO) MD5: f10b404d8f471ec1ac7f26f838f1d259");

    // save
    auto home = getenv("HOME");
    if (!home) {
        throw std::runtime_error { "Can't get HOME environment variable" };
    }

    auto path = std::filesystem::path { home } / ".staticlinux" / name;
    if (!std::filesystem::exists(path) && !std::filesystem::create_directories(path)) {
        throw std::runtime_error { std::format("Can't create path: {}", path.string()) };
    }

    auto path_str = (path / filePath).string();
    auto fp = std::unique_ptr<FILE, decltype(&fclose)> { fopen(path_str.c_str(), "wb"), &fclose };
    if (!fp) {
        throw std::runtime_error { std::format("Can't write file: {}", path_str) };
    }

    if (fwrite(data.data(), data.size(), 1, fp.get()) < 1) {
        throw std::runtime_error { std::format("Write file '{}' failed", path_str) };
    }
    status("Save to ~/.staticlinux/{}/{}", name, filePath);

    // TODO: add symbol
    status("(TODO): Add symbol link: ~/.staticlinux/bin/{}", std::filesystem::path { filePath }.filename().c_str());
}

export task_t<void> pull_async(int argc, const char* argv[])
{
    auto options = parse_options(argc, argv);
    if (options.help) {
        print_help();
        co_return;
    }

    if (argc-- == 0) {
        fatal_error("NAME parameter is required.");
    }

    auto str = std::string { *argv++ };

    auto pathIt = str.find('/');
    if (pathIt == std::string::npos) {
        fatal_error("PATH parameter is required.");
    }

    auto versionIt = str.find(':', pathIt + 1);
    if (versionIt == std::string::npos) {
        fatal_error("VERSION parameter is required.");
    }

    auto name = str.substr(0, pathIt);
    if (name.empty()) {
        fatal_error("NAME parameter is required.");
    }

    auto path = str.substr(pathIt + 1, versionIt - pathIt - 1);
    if (path.empty()) {
        fatal_error("PATH parameter is required.");
    }

    auto version = str.substr(versionIt + 1);
    if (version.empty()) {
        fatal_error("VERSION parameter is required.");
    }

    co_await pull_async(std::move(name), std::move(version), std::move(path));
}