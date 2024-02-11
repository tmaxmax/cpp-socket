#ifndef TERMCHAT_RECEIVE_H
#define TERMCHAT_RECEIVE_H

#include <cstddef>
#include <vector>

#include "protocol.h"
#include "socket.h"

struct ReceiveResult {
    std::optional<std::string> message;
    bool is_connected;
};

inline ReceiveResult receive(Receiver& r, std::vector<std::byte>& buf) {
    buf.resize(proto::header_size);
    if (!r.recv(buf)) {
        return {.is_connected = false};
    }

    const auto maybe_len = proto::unpack_header(buf);
    if (!maybe_len.has_value()) {
        return {.is_connected = true};
    }

    buf.resize(*maybe_len);
    if (!r.recv(buf)) {
        return {.is_connected = false};
    }

    return {.is_connected = true, .message = proto::unpack(buf, *maybe_len)};
}

#endif // TERMCHAT_RECEIVE_H