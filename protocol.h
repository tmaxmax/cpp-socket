#ifndef TERMCHAT_PROTOCOL_H
#define TERMCHAT_PROTOCOL_H

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

class Message {
public:
    // Shorthand for dynamic_cast. Use this to get the concrete message.
    template <typename T> T* as() { return dynamic_cast<T*>(this); }
    template <typename T> const T* as() const { return dynamic_cast<const T*>(this); }

    // Packs the message into a format which can be sent over the wire.
    // The result is written to the given vector.
    // Pack throws if the message content is invalid.
    // The result contains the message header.
    virtual void pack(std::vector<std::byte>&) const = 0;
    // Unpacks the given data into the struct the method is called on.
    // Returns whether the data was of the correct format for the message
    // type or not.
    // The data should not contain the message header.
    virtual bool unpack(std::span<const std::byte>) noexcept = 0;

    virtual ~Message() = default;
};

struct ClientMessage : public Message {
    std::string content;
    // The user to send the message to.
    // Given only if is_private is true.
    std::optional<std::string> user_name;
    bool is_private;

    void pack(std::vector<std::byte>&) const override;
    bool unpack(std::span<const std::byte>) noexcept override;
};

// ServerMessage represents any message sent by the server.
// These are teoretically of multiple types: registration error or succes,
// private message, broadcast etc., so we should – in theory – make more
// message types.
//
// In this case the only consumer is the client we create, which is a CLI
// application. Thus we know ahead of time how the output should look like,
// which means we can format the message directly on the server – the same
// way a classic web application sends HTML to a browser.
//
// By just sending what should be outputted by the client in the terminal,
// we simplify both the client and the protocol.
struct ServerMessage : public Message {
    std::string content;

    void pack(std::vector<std::byte>&) const override;
    bool unpack(std::span<const std::byte>) noexcept override;
};

struct ClientRegistration : public Message {
    std::string user_name;

    void pack(std::vector<std::byte>&) const override;
    bool unpack(std::span<const std::byte>) noexcept override;
};

struct Disconnect : public Message {
    void pack(std::vector<std::byte>&) const override;
    bool unpack(std::span<const std::byte>) noexcept override;
};

class Header {
public:
    // How many bytes should be received in order
    // to have fully read the header.
    static const std::size_t size;

    // Gives an intialized value of the correct message for the header
    // and how many more bytes should be received to have read the full message.
    // Returns nothing if the header contains invalid data.
    // Make sure to receive first Header::size() bytes.
    static std::optional<Header> parse(std::span<const std::byte>);

    std::unique_ptr<Message> message;
    std::size_t length;
};

#endif // TERMCHAT_PROTOCOL_H