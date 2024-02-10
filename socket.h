#ifndef TERMCHAT_SOCKET_H
#define TERMCHAT_SOCKET_H

#include <cstddef>
#include <memory>
#include <span>
#include <string>

// A set of abstractions over the sockets API. It is not meant to be fully featured
// but to only support the use-cases of the application.
//
// None of the abstractions here are thread-safe – callers should ensure synchronization.

class ServerClient;

enum class ServerClientStatus { New, PendingData };

struct ServerPollResult;

class Server {
private:
    struct Private;
    std::unique_ptr<Private> m;

public:
    // Creates a new server which listens on the given port.
    // If the port is less than 1024 or another error occurs,
    // the constructor throws.
    Server(unsigned short port);

    Server() = delete;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = default;
    Server& operator=(Server&&) = default;

    // Polls the server for new connections and the already existing
    // connections for data.
    void poll(std::vector<ServerPollResult>&);
    // Remove this client from the server.
    //
    // The Server keeps inside a registry of all the accepted clients
    // in order to be able to poll them for data. ServerClients which
    // have disconnected or should be disconnected must be removed from
    // the Server's registry aswell. It is not mandatory to call
    // close() on the ServerClient before – when all references to it
    // disappear it will be automatically closed.
    void remove(const ServerClient&);
    // Closes the server and prevents any subsequent sends or recvs
    // on any of its ServerClients.
    // Multiple calls to shutdown() will throw an error.
    void shutdown();

    ~Server();
};

class Receiver {
protected:
    virtual ~Receiver() = default;

public:
    // Receives exactly res.size() bytes from the client, blocking if necessary.
    // Returns true if the client is still connected, false if it disconnected.
    // Throws if the receive fails.
    virtual bool recv(std::vector<std::byte>& res) = 0;
};

class Sender {
protected:
    virtual ~Sender() = default;

public:
    // Send the given bytes to the client.
    // It is ensured that all bytes are sent.
    // Throws if the send doesn't succeed.
    virtual void send(std::span<const std::byte>) = 0;
};

class ServerClient : public Receiver, public Sender {
private:
    struct Private;
    std::shared_ptr<Private> m;

    friend class Server;

    explicit ServerClient(Private*);

public:
    // This is required in order to be able to put it in a
    // vector on which resize() is called. A ServerClient
    // shouldn't be created using the default constructor.
    ServerClient() = default;
    ServerClient(const ServerClient&) = default;
    ServerClient& operator=(const ServerClient&) = default;
    ServerClient(ServerClient&&) = default;
    ServerClient& operator=(ServerClient&&) = default;

    void send(std::span<const std::byte>) override;
    bool recv(std::vector<std::byte>& res) override;

    // Returns an unique ID associated with this client, given by the server.
    // It is useful because multiple clients can have the same IP address.
    std::size_t id() const noexcept;
    // Returns the IP address of the client.
    std::string address() const noexcept;

    // Closes the connection to this client.
    // Multiple calls to close() will throw an error.
    void close();

    ~ServerClient();
};

struct ServerPollResult {
    ServerClient client;
    ServerClientStatus status;
};

class Client : public Receiver, public Sender {
private:
    int m_fd;

public:
    // Creates a client which connects to the given address.
    // Throws if a connection error occurs.
    Client(std::string ip, unsigned short port);

    Client() = delete;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = default;
    Client& operator=(Client&&) = default;

    void send(std::span<const std::byte>) override;
    bool recv(std::vector<std::byte>& res) override;

    // Closes the connection to the server.
    // Multiple calls to close() will throw an error.
    void close();

    ~Client();
};

#endif