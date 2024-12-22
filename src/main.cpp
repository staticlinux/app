import consts;
import log;
import pull;

#include <cstdio>
#include <cstring>

#define DOCS_BASE_LINK "https://docs.staticlinux.org/app"

struct Options
{
    bool help{};
};

static Options parse_options(int &argc, const char **&argv)
{
    auto options = Options{};
    while (argc && **argv == '-')
    {
        if (!strcmp(*argv + 1, "h") || !strcmp(*argv + 1, "-help"))
        {
            options.help = true;
            --argc;
            ++argv;
        }
        else
        {
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

int main(int argc, const char *argv[])
{
    --argc;
    ++argv;

    auto options = parse_options(argc, argv);
    if (options.help)
    {
        print_help();
        return 0;
    }

    if (argc == 0)
    {
        fatal_error("command is required");
    }

    if (!strcmp(*argv, "pull"))
    {
        pull(--argc, ++argv);
    }
    else
    {
        fatal_error("unknown command: {}", *argv);
    }
    return 0;
}