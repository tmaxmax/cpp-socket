#include "server.hxx"

#include <future>
#include <stdexcept>

#include "logger.hxx"

static constexpr std::int32_t max_packet{4096};

static auto GetClientIPAddress(const Server::ClientConnection& client) -> std::string {
    const auto client_ptr = client.lock();
    if (client_ptr == nullptr) {
        return "";
    }
    return fmt::format("{}:{}", client_ptr->GetClientAddr(), client_ptr->GetClientPort());
}

Server::Server()
    : m_is_listening{false} {
    if (!m_socket.Initialize()) {
        throw std::runtime_error(m_socket.DescribeError());
    }
}

struct IOThreadData {
private:
    using ClientConnectionAddr = Server::ClientConnection::element_type*;
public:
    IOThreadData(std::future<void> done, ClientConnectionAddr addr)
        : done(std::move(done)), addr(addr) {}

    std::future<void> done;
    ClientConnectionAddr addr;
};

class ClientBlockGuard {
public:
    explicit ClientBlockGuard(Server::ClientConnection client)
        : m_client(std::move(client)) {
        auto client_ptr = m_client.lock();
        if (client_ptr != nullptr) {
            client_ptr->SetNonblocking();
        }
    }
    ~ClientBlockGuard() noexcept {
        auto client_ptr = m_client.lock();
        if (client_ptr != nullptr) {
            client_ptr->SetBlocking();
        }
    }

private:
    Server::ClientConnection m_client;
};

auto Server::Listen(std::uint16_t port) -> void {
    if (!m_socket.Listen("0.0.0.0", port)) {
        throw std::runtime_error(m_socket.DescribeError());
    }
    m_is_listening = true;

    auto accept_ended = std::async(std::launch::async, [&]() {
        while (m_is_listening) {
            if (!m_socket.Select(0, 1)) {
                continue;
            }
            auto new_client = m_socket.Accept();
            if (new_client == nullptr) {
                throw std::runtime_error(m_socket.DescribeError());
            }
            m_clients.emplace_front(new_client);
            for (const auto& handler : m_connect_handlers) {
                handler(m_clients.front());
            }
            teo::Log("Client {} connected!", GetClientIPAddress(m_clients.front()));
        }
    });

    std::forward_list<IOThreadData> io_threads;
    while (true) {
        for (auto client_it = m_clients.cbefore_begin(); std::next(client_it) != m_clients.end();) {
            ClientConnection client_ptr = *std::next(client_it);
            const auto client = client_ptr.lock();
            if (client == nullptr) {
                teo::Log("Client {} expired, deleting...");

                m_clients.erase_after(client_it);
                continue;
            }
            const auto client_addr = GetClientIPAddress(client);

            if (!client->IsSocketValid()) {
                teo::Log("Client {}'s file descriptor invalid, deleting...", client_addr);

                m_clients.erase_after(client_it);
                continue;
            }

            io_threads.remove_if([](const auto& io) {
                return io.done.wait_for(std::chrono::nanoseconds{}) == std::future_status::ready;
            });
            for (const auto& io : io_threads) {
                if (io.addr == client.get()) {
                    goto iterate;
                }
            }
            io_threads.emplace_front(std::async(std::launch::async, [this, client, client_addr]() {
                std::string recv_message;
                {
                    ClientBlockGuard guard(client);
                    recv_message = ReceiveMessage(client);
                }
                if (recv_message.empty()) {
                    return;
                }
                teo::Log("Received message \"{}\" from client {}!", recv_message, client_addr);
                for (const auto& handler : m_message_handlers) {
                    handler(client, recv_message);
                }
            }), client.get());

            iterate:
            ++client_it;
        }
        if (!m_is_listening) {
            accept_ended.wait();
            for (const auto& io : io_threads) {
                io.done.wait();
            }
            m_clients.clear();
            break;
        }
    }
}

auto Server::Close() -> void {
    if (!m_socket.Shutdown(CPassiveSocket::CShutdownMode::Both)) {
        throw std::runtime_error(m_socket.DescribeError());
    }
    m_is_listening = false;
}

Server::~Server() {
    m_socket.Close();
}

auto Server::OnConnect(Server::ConnectHandler&& handler) -> void {
    m_connect_handlers.push_back(std::move(handler));
}

auto Server::OnMessage(Server::MessageHandler&& handler) -> void {
    m_message_handlers.push_back(std::move(handler));
}

auto Server::ReceiveMessage(const Server::ClientConnection& connection) -> std::string {
    auto conn_ptr = connection.lock();
    if (conn_ptr == nullptr) {
        throw std::runtime_error("Connection closed");
    }
    switch (conn_ptr->Receive(max_packet)) {
    case -1:
        throw std::runtime_error(conn_ptr->DescribeError());
    case 0:
        teo::Log("Client {} disconnected, closing it's file descriptor...", GetClientIPAddress(conn_ptr));
        conn_ptr->Close();
        return "";
    default:
        std::string recv_message(reinterpret_cast<const char*>(conn_ptr->GetData()));
        recv_message.resize(conn_ptr->GetBytesReceived());
        return std::move(recv_message);
    }
}

auto Server::SendMessage(const Server::ClientConnection& connection, std::string_view message) -> void {
    auto conn_ptr = connection.lock();
    if (conn_ptr == nullptr) {
        teo::Log("Client invalid");
        return;
    }
    const auto client_addr = GetClientIPAddress(conn_ptr);
    const auto bytes_sent = conn_ptr->Send(reinterpret_cast<const std::uint8_t*>(message.data()), message.size());
    switch (bytes_sent) {
    case -1:
        throw std::runtime_error(conn_ptr->DescribeError());
    case 0:
        teo::Log("Client {} disconnected, closing it's file descriptor...", client_addr);
        conn_ptr->Close();
        break;
    default:
        teo::Log("Sent {} bytes to {}.", bytes_sent, GetClientIPAddress(conn_ptr));
    }
}

auto Server::BroadcastMessage(std::string_view message) -> void {
    for (const auto& client : m_clients) {
        SendMessage(client, message);
    }
}
