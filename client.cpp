#include <cstddef>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "common.h"
#include "protocol.h"
#include "socket.h"

int main(int argc, char** argv) try {
    if (argc < 3) {
        std::cerr << "termchat: ip and port must be specified\n";
        return 1;
    }

    const unsigned short port = std::stoul(argv[2]);

    Client client(argv[1], port);
    std::vector<std::byte> buf;

    for (std::string s; std::getline(std::cin, s);) {
        ClientRegistration msg;
        msg.user_name = s;
        send_message(&client, &msg, buf);

        const auto received = recv_message(&client, buf);
        if (!received.is_connected) {
            std::cerr << "Server closed.\n";
            return 0;
        } else if (received.message == nullptr) {
            std::cerr << "Server sent invalid message (how?).\n";
            return 0;
        }

        std::cout << received.message->as<ServerMessage>()->content;
    }

    Disconnect dis;
    send_message(&client, &dis, buf);

    std::cerr << "Done\n";

} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
}