#ifndef TERMCHAT_PROTOCOL_H
#define TERMCHAT_PROTOCOL_H

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace proto {
void pack(std::string_view data, std::vector<std::byte>& out);

extern const std::size_t size_header;
std::optional<std::size_t> unpack_header(std::span<const std::byte> in) noexcept;
std::optional<std::string> unpack(std::span<const std::byte> in, std::size_t expected_len) noexcept;
} // namespace proto

#endif // TERMCHAT_PROTOCOL_H