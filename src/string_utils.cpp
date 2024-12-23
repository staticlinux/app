module;

#include <ranges>
#include <string>

export module string_utils;

export std::string trim(std::string_view str)
{
    auto v = str
        | std::views::drop_while(isspace)
        | std::views::reverse
        | std::views::drop_while(isspace)
        | std::views::reverse;
    return std::string { v.begin(), v.end() };
}

export std::string tolower(std::string_view str)
{
    auto v = str | std::views::transform([](auto c) { return tolower(c); });
    return std::string { v.begin(), v.end() };
}