module;

#include <format>

export module log;

export template <typename... Args>
void status(std::format_string<Args...> fmt, Args&&... args)
{
    fprintf(stdout, "%s\n", std::vformat(fmt.get(), std::make_format_args(args...)).c_str());
}

export template <typename... Args>
void fatal_error(std::format_string<Args...> fmt, Args&&... args)
{
    fprintf(stderr, "error: %s\n", std::vformat(fmt.get(), std::make_format_args(args...)).c_str());
    exit(1);
}