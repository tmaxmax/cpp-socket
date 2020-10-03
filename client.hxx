#ifndef CPP_SOCKET_CLIENT_HXX
#define CPP_SOCKET_CLIENT_HXX

#include <ActiveSocket.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

#if defined(__MINGW32__) || defined(__MINGW64__)
#undef SendMessage
#endif

class Client {
public:
    Client();
    ~Client();

    auto Connect(const char*, std::uint16_t) -> void;
    auto Disconnect() -> void;

    auto SendMessage(std::string_view) -> std::int32_t;
    auto ReceiveMessage() -> std::string;

private:
    CActiveSocket m_client;
};

#endif //CPP_SOCKET_CLIENT_HXX
