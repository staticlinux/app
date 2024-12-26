import consts;
import cppl;
import log;
import message_queue;
import pull;

#include <coroutine>
#include <cstdio>
#include <cstring>
#include <exception>

#define DOCS_BASE_LINK "https://docs.staticlinux.org/app"

using cppl::task_state_t;
using cppl::task_t;

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
    fprintf(stdout, R"(StaticLinux app management tool
Usage: app [OPTIONS] <command>

Options:
    -h,--help                   Print this help message and exit

Subcommands:
    pull                        Download app from internet

For more information, please visit %s
)",
        DOC_BASE_LINK);
}

task_t<int> main_async(int argc, const char* argv[])
{
    --argc;
    ++argv;

    auto options = parse_options(argc, argv);
    if (options.help) {
        print_help();
        co_return 0;
    }

    if (argc == 0) {
        fatal_error("command is required");
    }

    if (!strcmp(*argv, "pull")) {
        co_await pull_async(--argc, ++argv);
    } else {
        fatal_error("unknown command: {}", *argv);
    }
    co_return 0;
}

int main(int argc, const char* argv[])
{
    try {
        return message_queue_t::current().wait(main_async(argc, argv));
    } catch (const std::exception& ex) {
        fatal_error("{}", ex.what());
    }
}