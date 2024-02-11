#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "protocol.h"

static void pack_u64(uint64_t v, std::vector<std::byte>& out) {
    const auto nv = htonll(v);
    const auto nv_addr = reinterpret_cast<const std::byte*>(&nv);
    out.insert(out.end(), nv_addr, nv_addr + sizeof nv);
}

void proto::pack(std::string_view v, std::vector<std::byte>& out) {
    pack_u64(v.size(), out);

    const auto v_addr = reinterpret_cast<const std::byte*>(v.data());
    out.insert(out.end(), v_addr, v_addr + v.size());
}

const std::size_t proto::header_size = sizeof(uint64_t);

std::optional<std::size_t> proto::unpack_header(std::span<const std::byte> in) noexcept {
    if (in.size() < proto::header_size) {
        return std::nullopt;
    }

    const auto v = ntohll(*reinterpret_cast<const uint64_t*>(in.data()));

    if (v > 4096) { // limit maximum length
        return std::nullopt;
    }

    in = in.subspan(proto::header_size);

    return v;
}

std::optional<std::string>
proto::unpack(std::span<const std::byte> in, std::size_t expected_len) noexcept {
    if (in.size() < expected_len) {
        return std::nullopt;
    }

    const auto addr = reinterpret_cast<const char*>(in.data());
    std::string s(addr, addr + expected_len);

    in = in.subspan(expected_len);

    return s;
}
