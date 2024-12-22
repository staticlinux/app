module;

#include <format>

export module log;

export void status(std::string_view fmt, auto &&...args)
{
    fprintf(stdout, "%s\n", std::vformat(fmt, std::make_format_args(std::forward<decltype(args)>(args)...)).c_str());
}

export void fatal_error(std::string_view fmt, auto &&...args)
{
    fprintf(stderr, "error: %s\n", std::vformat(fmt, std::make_format_args(std::forward<decltype(args)>(args)...)).c_str());
    exit(1);
}