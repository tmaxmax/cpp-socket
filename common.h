#ifndef TERMCHAT_COMMON_H
#define TERMCHAT_COMMON_H

#include <memory>
#include <optional>
#include <utility>

#include "protocol.h"
#include "socket.h"

inline void send_message(Sender& s, const Message& m, std::vector<std::byte>& buf) {
    buf.resize(0);
    m.pack(buf);
    s.send(buf);
}

struct ReceiveResult {
    bool is_connected;
    std::unique_ptr<Message> message;
};

inline ReceiveResult recv_message(Receiver& r, std::vector<std::byte>& buf) {
    buf.resize(Header::size);
    if (!r.recv(buf)) {
        return {.is_connected = false};
    }

    auto header = Header::parse(buf);
    if (!header.has_value()) {
        return {.is_connected = true};
    }

    buf.resize(header->length);
    if (!r.recv(buf)) {
        return {.is_connected = false};
    }

    if (!header->message->unpack(buf)) {
        return {.is_connected = true};
    }

    return {.is_connected = true, .message = std::move(header->message)};
}

#endif // TERMCHAT_COMMON_H