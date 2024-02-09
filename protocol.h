#ifndef TERMCHAT_PROTOCOL_H
#define TERMCHAT_PROTOCOL_H

#include <span>
#include <string_view>
#include <vector>

void pack_string(std::string_view v, std::vector<std::byte>& out);
std::size_t parse_len(std::span<const std::byte> input);
std::string_view as_string(std::span<const std::byte> input);

#endif // TERMCHAT_PROTOCOL_H