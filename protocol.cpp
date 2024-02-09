#include <arpa/inet.h>

#include "protocol.h"

void pack_string(std::string_view v, std::vector<std::byte>& out) {
    out.resize(0);

    const auto len = htonll(v.size());
    const auto len_addr = reinterpret_cast<const std::byte*>(&len);

    out.insert(std::end(out), len_addr, len_addr + sizeof len);

    const auto v_addr = reinterpret_cast<const std::byte*>(v.data());
    out.insert(std::end(out), v_addr, v_addr + v.size());
}

std::size_t parse_len(std::span<const std::byte> input) {
    return ntohll(*reinterpret_cast<const std::size_t*>(input.data()));
}

std::string_view as_string(std::span<const std::byte> input) {
    return std::string_view{reinterpret_cast<const char*>(input.data()), input.size()};
}