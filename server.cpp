#include <cstddef>
#include <exception>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "common.h"
#include "protocol.h"
#include "socket.h"

int main(int argc, char** argv) try {
    if (argc < 2) {
        std::cerr << "termchat: no port specified\n";
        return 1;
    }

    const unsigned short port = std::stoul(argv[1]);

    Server server(port);
    std::vector<ServerPollResult> clients;
    std::vector<std::byte> buf;

    while (true) {
        server.poll(clients);
        for (auto& [client, _] : clients) {
            const auto received = recv_message(&client, buf);
            if (!received.is_connected) {
                std::cerr << "Client " << client.id() << " disconnected\n";
                server.remove(client);
                continue;
            }
            if (received.message == nullptr) {
                std::cerr << "Client " << client.id() << " sent an invalid message\n";
                continue;
            }

            if (const auto m = received.message->as<ClientRegistration>(); m != nullptr) {
                std::cerr << "Client " << client.id() << " sent: " << m->user_name << '\n';

                std::ostringstream ss;
                ss << "You sent: " << std::quoted(m->user_name) << ".\nGive me something new: ";

                ServerMessage msg;
                msg.content = ss.str();

                send_message(&client, &msg, buf);
                continue;
            }
            if (const auto m = received.message->as<Disconnect>(); m != nullptr) {
                std::cerr << "Client " << client.id() << " disconnected.\n";
                server.remove(client);
                continue;
            }

            std::cerr << "Client " << client.id() << " sent an unhandled message type.\n";
        }
    }
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
}