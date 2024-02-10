#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "protocol.h"

//
// Packing & unpacking utilities.
// The packing utils append the byte representation of the values to the vector.
// The unpacking utils extract the required bytes from the span and then shrink the span forward.
//

void pack_bool(bool v, std::vector<std::byte>& out) { out.push_back(static_cast<std::byte>(v)); }

void pack_u64(uint64_t v, std::vector<std::byte>& out) {
    const auto nv = htonll(v);
    const auto nv_addr = reinterpret_cast<const std::byte*>(&nv);
    out.insert(out.end(), nv_addr, nv_addr + sizeof nv);
}

void pack_string(std::string_view v, std::vector<std::byte>& out) {
    pack_u64(v.size(), out);

    const auto v_addr = reinterpret_cast<const std::byte*>(v.data());
    out.insert(out.end(), v_addr, v_addr + v.size());
}

std::optional<bool> unpack_bool(std::span<const std::byte>& in) noexcept {
    if (in.size() < 1) {
        return std::nullopt;
    }

    const auto v = static_cast<unsigned char>(in[0]);
    if (v != 0 && v != 1) {
        return std::nullopt;
    }

    in = in.subspan(1);

    return static_cast<bool>(v);
}

std::optional<uint64_t> unpack_u64(std::span<const std::byte>& in) noexcept {
    constexpr auto num_bytes = sizeof(uint64_t);

    if (in.size() < num_bytes) {
        return std::nullopt;
    }

    const auto v = ntohll(*reinterpret_cast<const uint64_t*>(in.data()));

    in = in.subspan(num_bytes);

    return v;
}

std::optional<std::string> unpack_string(std::span<const std::byte>& in) noexcept {
    const auto size = unpack_u64(in);
    if (!size.has_value() || in.size() < *size) {
        return std::nullopt;
    }

    const auto addr = reinterpret_cast<const char*>(in.data());
    std::string s(addr, addr + *size);

    in = in.subspan(*size);

    return s;
}

//
// The header
//

enum Code : uint8_t {
    CodeClientMessage,
    CodeServerMessage,
    CodeClientRegistration,
    CodeRegistrationSuccess,
    CodeDisconnect
};

void pack_code(Code v, std::vector<std::byte>& out) { out.push_back(static_cast<std::byte>(v)); }

std::optional<Code> unpack_code(std::span<const std::byte>& in) noexcept {
    if (in.size() < 1) {
        return std::nullopt;
    }

    const auto v = static_cast<unsigned char>(in[0]);
    switch (v) {
    case CodeClientMessage:
    case CodeServerMessage:
    case CodeClientRegistration:
    case CodeRegistrationSuccess:
    case CodeDisconnect:
        in = in.subspan(1);

        return static_cast<Code>(v);
    }

    return std::nullopt;
}

// The header contains the message code and the message length
// as a 64-bit unsigned int.
const std::size_t Header::size = sizeof(Code) + sizeof(uint64_t);

std::optional<Header> Header::parse(std::span<const std::byte> input) {
    // Prevent malicious payloads. If we would accept any message length
    // someone could send us 2^64 as the length and our code would attempt
    // to allocate that much memory.
    constexpr auto max_length = 4096;

    if (input.size() < Header::size) {
        return std::nullopt;
    }

    const auto code = unpack_code(input);
    if (!code.has_value()) {
        return std::nullopt;
    }

    const auto length = unpack_u64(input);
    if (!length.has_value() || *length > max_length) {
        return std::nullopt;
    }

    Header h;
    h.length = *length;

    switch (*code) {
    case CodeClientMessage:
        h.message = std::unique_ptr<Message>{new ClientMessage()};
        break;
    case CodeServerMessage:
        h.message = std::unique_ptr<Message>{new ServerMessage()};
        break;
    case CodeClientRegistration:
        h.message = std::unique_ptr<Message>{new ClientRegistration()};
        break;
    case CodeRegistrationSuccess:
        h.message = std::unique_ptr<Message>{new RegistrationSuccess()};
        break;
    case CodeDisconnect:
        h.message = std::unique_ptr<Message>{new Disconnect()};
        break;
    }

    return h;
}

//
// Packing & unpacking for all message types.
//
// The pack_start and pack_end functions should be called at the start
// respectively at the very end of every pack() functions. These build
// the header – pack_start adds the message code and leaves space for
// the length, pack_end writes the length into that space based on how
// many bytes are in the vector. This avoids having to manually calculate
// the length in each pack() implementation.
//

static void pack_start(Code msg_code, std::vector<std::byte>& out) {
    pack_code(msg_code, out);
    out.insert(out.end(), Header::size - sizeof(Code), std::byte{});
}

static void pack_end(std::vector<std::byte>& out) {
    const uint64_t length = out.size() - Header::size;
    *reinterpret_cast<uint64_t*>(&out[sizeof(Code)]) = htonll(length);
}

void ClientMessage::pack(std::vector<std::byte>& out) const {
    if (is_private && !user_name.has_value()) {
        throw std::runtime_error("private client message should specify the user it is sent to");
    }

    pack_start(CodeClientMessage, out);
    pack_bool(is_private, out);
    if (is_private) {
        pack_string(*user_name, out);
    }
    pack_string(content, out);
    pack_end(out);
}

bool ClientMessage::unpack(std::span<const std::byte> in) noexcept {
    const auto maybe_is_private = unpack_bool(in);
    if (!maybe_is_private.has_value()) {
        return false;
    }

    is_private = *maybe_is_private;

    if (is_private) {
        const auto maybe_user_name = unpack_string(in);
        if (!maybe_user_name.has_value()) {
            return false;
        }

        user_name = std::move(maybe_user_name);
    }

    const auto maybe_content = unpack_string(in);
    if (!maybe_content.has_value()) {
        return false;
    }

    content = std::move(*maybe_content);

    return true;
}

void ServerMessage::pack(std::vector<std::byte>& out) const {
    pack_start(CodeServerMessage, out);
    pack_string(content, out);
    pack_end(out);
}

bool ServerMessage::unpack(std::span<const std::byte> in) noexcept {
    const auto maybe_content = unpack_string(in);
    if (!maybe_content.has_value()) {
        return false;
    }

    content = std::move(*maybe_content);

    return true;
}

void ClientRegistration::pack(std::vector<std::byte>& out) const {
    pack_start(CodeClientRegistration, out);
    pack_string(user_name, out);
    pack_end(out);
}

bool ClientRegistration::unpack(std::span<const std::byte> in) noexcept {
    const auto maybe_user_name = unpack_string(in);
    if (!maybe_user_name.has_value()) {
        return false;
    }

    user_name = std::move(*maybe_user_name);

    return true;
}

void Disconnect::pack(std::vector<std::byte>& out) const {
    pack_start(CodeDisconnect, out);
    pack_end(out);
}

bool Disconnect::unpack(std::span<const std::byte>) noexcept { return true; }

void RegistrationSuccess::pack(std::vector<std::byte>& out) const {
    pack_start(CodeRegistrationSuccess, out);
    pack_end(out);
}

bool RegistrationSuccess::unpack(std::span<const std::byte>) noexcept { return true; }