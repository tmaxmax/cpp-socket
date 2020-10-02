#ifndef CPP_SOCKET_LOGGER_HXX
#define CPP_SOCKET_LOGGER_HXX

#include <fmt/format.h>
#include <mutex>

namespace teo {
/// Log logs to stderr, appending a newline at the end. Supports format strings as in the fmt package.
template<typename S, typename... Args>
auto Log(const S& format_str, Args&&... args) {
    static std::mutex mu;

    std::scoped_lock lock(mu);
    fmt::print(stderr, format_str, std::forward<Args>(args)...);
    fmt::print(stderr, "\n");
}
}

#endif //CPP_SOCKET_LOGGER_HXX
