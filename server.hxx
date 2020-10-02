#ifndef CPP_SOCKET_SERVER_HXX
#define CPP_SOCKET_SERVER_HXX

#include <ActiveSocket.h>
#include <PassiveSocket.h>

#include <atomic>
#include <forward_list>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

class Server {
public:
    using ClientConnectionPtr = std::shared_ptr<CActiveSocket>;
    using ClientConnection = ClientConnectionPtr::weak_type;
    using MessageHandler = std::function<void(const ClientConnection&, std::string_view)>;
    using ConnectHandler = std::function<void(const ClientConnection&)>;

    Server();
    ~Server();

    auto Listen(std::uint16_t) -> void;
    auto Close() -> void;

    auto OnMessage(MessageHandler&&) -> void;
    auto OnConnect(ConnectHandler&&) -> void;
    static auto SendMessage(const ClientConnection&, std::string_view) -> void;
    static auto ReceiveMessage(const ClientConnection&) -> std::string;
    auto BroadcastMessage(std::string_view) -> void;

private:
    CPassiveSocket m_socket;
    std::atomic<bool> m_is_listening;
    std::forward_list<ClientConnectionPtr> m_clients;
    std::vector<ConnectHandler> m_connect_handlers;
    std::vector<MessageHandler> m_message_handlers;
};

#endif //CPP_SOCKET_SERVER_HXX
