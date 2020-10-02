#include "../logger.hxx"
#include "../server.hxx"

static auto GetClientAddr(const Server::ClientConnection& client) -> std::string {
    auto c = client.lock();
    if (c == nullptr) {
        return "";
    }
    return fmt::format("{}:{}", c->GetClientAddr(), c->GetClientPort());
}

int main() {
    try {
        Server server;
        server.OnMessage([&server](auto client, auto message) {
            if (message == "sarmale") {
                Server::SendMessage(client, "De care sarmale?");
                auto resp{Server::ReceiveMessage(client)};
                while (resp != "carne") {
                    Server::SendMessage(client, "Nu stiu ce ai ales, mai incearca o data!");
                    resp = std::move(Server::ReceiveMessage(client));
                }
                Server::SendMessage(client, "Ai ales corect! Poftim o sarma.");
                return;
            }
            if (message == "close") {
                server.BroadcastMessage("Server closed!");
                server.Close();
                return;
            }
            if (message.starts_with("bc ")) {
                if (!message.substr(3).empty()) {
                    server.BroadcastMessage(fmt::format("Client {} said: \"{}\"", GetClientAddr(client), message.substr(3)));
                }
                return;
            }
            Server::SendMessage(client, message);
        });
        constexpr std::uint16_t port{8363};
        teo::Log("Server listening on port {}", port);
        server.Listen(port);
        teo::Log("Server closed!");
    } catch (const std::exception& e) {
        teo::Log("{}", e.what());
    }
}
