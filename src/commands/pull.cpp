module;

#include <cassert>
#include <cstdio>
#include <cstring>
#include <format>

import consts;
import log;

export module pull;

namespace
{
    struct Options
    {
        bool help{};
    };

    Options parse_options(int &argc, const char **&argv)
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

    void print_help()
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

    void pull(std::string name, std::string version, std::string filePath)
    {
        assert(!name.empty());
        assert(!version.empty());
        assert(!filePath.empty());

        auto arch = sizeof(void *) == 4 ? "x86" : "amd64";
        auto downloadPath = std::format("{}/{}/{}/{}/{}", APP_DOWNLOAD_BASE_LINK, name, version, arch, filePath);
        status("Pulling from {}", downloadPath);
    }
}

export void pull(int argc, const char *argv[])
{
    auto options = parse_options(argc, argv);
    if (options.help)
    {
        print_help();
    }

    if (argc-- == 0)
    {
        fatal_error("NAME parameter is required.");
    }

    auto str = std::string{*argv++};

    auto pathIt = str.find('/');
    if (pathIt == std::string::npos)
    {
        fatal_error("PATH parameter is required.");
    }

    auto versionIt = str.find(':', pathIt + 1);
    if (versionIt == std::string::npos)
    {
        fatal_error("VERSION parameter is required.");
    }

    auto name = str.substr(0, pathIt);
    if (name.empty())
    {
        fatal_error("NAME parameter is required.");
    }

    auto path = str.substr(pathIt + 1, versionIt - pathIt - 1);
    if (path.empty())
    {
        fatal_error("PATH parameter is required.");
    }

    auto version = str.substr(versionIt + 1);
    if (version.empty())
    {
        fatal_error("VERSION parameter is required.");
    }

    pull(std::move(name), std::move(version), std::move(path));
}