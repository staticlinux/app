module;

#include <cassert>
#include <coroutine>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <regex>
#include <sys/stat.h>
#include <yaml-cpp/yaml.h>

import consts;
import cppl;
import http_client;
import log;
import lzma;
import md5;
import read_stream;

using cppl::task_state_t;
using cppl::task_t;

export module pull;

struct Options {
    bool help {};
};

struct Metadata {
    struct File {
        std::string md5 {};
        int mode {};
        size_t size {};
        std::string filepath {};
    };

    std::vector<File> files {};
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

static int parse_string_permission(std::string_view permission)
{
    if (permission.size() != 9) {
        throw std::runtime_error { std::format("bad permission string: {}", permission) };
    }

    int mode {};
    for (int i = 0; i < 3; ++i) {
        mode <<= 1;
        if (auto c = permission[i * 3]; c == 'r') {
            mode |= 1;
        } else if (c != '-') {
            throw std::runtime_error { std::format("bad permission string: {}", permission) };
        }
        mode <<= 1;
        if (auto c = permission[i * 3 + 1]; c == 'w') {
            mode |= 1;
        } else if (c != '-') {
            throw std::runtime_error { std::format("bad permission string: {}", permission) };
        }
        mode <<= 1;
        if (auto c = permission[i * 3 + 2]; c == 'x') {
            mode |= 1;
        } else if (c != '-') {
            throw std::runtime_error { std::format("bad permission string: {}", permission) };
        }
    }
    return mode;
}

static task_t<Metadata> pull_metadata_async(read_stream_t& read_stream, size_t metadata_file_len)
{
    trace("Download metadata ...");

    // Read metadata
    auto data = co_await read_stream.read_async(metadata_file_len);

    trace("Depress metadata ...");
    auto rawdata = lzma_decompress(data);

    trace("Parse metadata ...");
    Metadata metadata {};
    auto doc = YAML::Load(std::string { rawdata.data(), rawdata.data() + rawdata.size() });
    const auto& files = doc["files"];
    auto file_regex = std::regex { R"(^([0-9a-f]+)\s+-(([rwx-]{3}){3})\s+(\d+)\s+([^\r\n]+)$)" };
    for (const auto& file : files) {
        auto line = file.as<std::string>();
        auto res = std::smatch {};
        if (!std::regex_match(line, res, file_regex)) {
            throw std::runtime_error { std::format("Bad file item: {}", line) };
        }
        metadata.files.push_back({
            .md5 = res[1].str(),
            .mode = parse_string_permission(res[2].str()),
            .size = (size_t)atoi(res[4].str().c_str()),
            .filepath = std::move(res[5].str()),
        });
    }
    co_return metadata;
}

static task_t<void> pull_async(std::string name, std::string version, std::string filepath)
{
    assert(!name.empty());
    assert(!version.empty());
    assert(!filepath.empty());

    auto arch = sizeof(void*) == 4 ? "x86" : "amd64";
    auto downloadPath = std::format("{0}/{1}/{2}/{3}/{1}-{2}-{3}.slp", APP_DOWNLOAD_BASE_LINK, name, version, arch);
    status("Pulling from {}", downloadPath);

    // Download metadata.
    auto [header, read_stream] = co_await http_get_header_async(downloadPath);

    // Read package file header
    const size_t PACKAGE_HEADER_LEN = 8;
    auto package_file_header = co_await read_stream.read_async(PACKAGE_HEADER_LEN);

    // Verify magic number
    if (memcmp(package_file_header.data(), "\xF1SLP\x00", 5)) {
        throw std::runtime_error { "Invalid package file" };
    }

    // Get metadata file length
    auto metadata_file_len = *(uint32_t*)(package_file_header.data() + 4) & ~0xff;

    // Read metadata
    auto metadata = co_await pull_metadata_async(read_stream, metadata_file_len);

    // Find out the file item.
    size_t firstByteOffset { PACKAGE_HEADER_LEN + metadata_file_len };
    size_t lastByteOffset {};
    const Metadata::File* pFile {};
    for (const auto& file : metadata.files) {
        if (file.filepath == filepath) {
            pFile = &file;
            lastByteOffset = firstByteOffset + file.size - 1;
            break;
        }
        firstByteOffset += file.size;
    }
    if (!lastByteOffset) {
        throw std::runtime_error { std::format("Can't find {} in package {}", filepath, name) };
    }

    // Download file.
    trace("Download file content, bytes: {}-{}", firstByteOffset, lastByteOffset);
    auto data = co_await http_get_async(downloadPath, { { "range", std::format("bytes={}-{}", firstByteOffset, lastByteOffset) } });
    status("Pull completed");

    trace("Decompress content");
    auto rawdata = lzma_decompress(data);
    status("Size: {}", rawdata.size());

    // Caculate md5.
    trace("Verify md5");
    auto md5 = md5_string(rawdata);
    status("MD5: {}", md5);

    // Make sure md5 is the same.
    if (md5 != pFile->md5) {
        throw std::runtime_error { "MD5 doesn't match please contact admin@staticlinux.org" };
    }

    // save
    auto home = getenv("HOME");
    if (!home) {
        throw std::runtime_error { "Can't get HOME environment variable" };
    }

    auto path = std::filesystem::path { home } / ".staticlinux" / name;
    if (!std::filesystem::exists(path) && !std::filesystem::create_directories(path)) {
        throw std::runtime_error { std::format("Can't create path: {}", path.string()) };
    }

    auto path_str = (path / filepath).string();
    auto fp = std::unique_ptr<FILE, decltype(&fclose)> { fopen(path_str.c_str(), "wb"), &fclose };
    if (!fp) {
        throw std::runtime_error { std::format("Can't write file: {}", path_str) };
    }

    if (fwrite(rawdata.data(), rawdata.size(), 1, fp.get()) < 1) {
        throw std::runtime_error { std::format("Write file '{}' failed", path_str) };
    }
    status("Save to ~/.staticlinux/{}/{}", name, filepath);

    // change mode
    if (chmod(path_str.c_str(), pFile->mode) < 0) {
        throw std::system_error { errno, std::system_category(), "chmod failed" };
    }

    // add symbol if this file is executable
    if (pFile->mode & 0111) {
        auto symbolLinkName = std::filesystem::path { filepath }.filename().string();
        status("Add symbol link: ~/.staticlinux/bin/{}", symbolLinkName);
        auto binpath = std::filesystem::path { home } / ".staticlinux" / "bin";
        if (!std::filesystem::exists(binpath) && !std::filesystem::create_directories(binpath)) {
            throw std::runtime_error { std::format("Can't create path: {}", binpath.string()) };
        }
        if (symlink(path_str.c_str(), (binpath / symbolLinkName).c_str()) < 0) {
            throw std::system_error { errno, std::system_category(), "symlink failed" };
        }
    }
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