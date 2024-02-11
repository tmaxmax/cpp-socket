#include <atomic>
#include <cstddef>
#include <exception>
#include <future>
#include <iostream>
#include <string>
#include <vector>

#include "protocol.h"
#include "receive.h"
#include "socket.h"

int main(int argc, char** argv) try {
    if (argc < 3) {
        std::cerr << "termchat: ip and port must be specified\n";
        return 1;
    }

    const unsigned short port = std::stoul(argv[2]);

    Client client(argv[1], port);
    std::atomic_flag is_server_closed;

    const auto send_done = std::async(std::launch::async, [&]() {
        std::vector<std::byte> buf;
        for (std::string s; std::getline(std::cin, s);) {
            buf.resize(0);
            proto::pack(s, buf);
            try {
                client.send(buf);
            } catch (const SocketError&) {
                try {
                    client.close();
                } catch (const SocketError&) {
                }
            }
        }
        if (!is_server_closed.test()) {
            buf.resize(0);
            proto::pack("", buf); // disconnect message is empty string
            try {
                client.send(buf);
            } catch (const SocketError&) {
            }
            try {
                client.close();
            } catch (const SocketError&) {
            }
        }
    });

    try {
        for (std::vector<std::byte> buf;;) {
            const auto recv = receive(client, buf);
            if (!recv.is_connected) {
                std::cout << "Server closed. Please quit the program.\n";
                break;
            }
            if (!recv.message.has_value()) {
                // this would be an invalid message from the server.
                // should not happen, so ignore.
                continue;
            }

            std::cout << *recv.message << std::flush;
        }
    } catch (const SocketError& e) {
        if (!e.bad_fd()) {
            std::cout << "Something wrong. Please quite the program.\n - error: " << e.what()
                      << "\n";
        }
    }

    send_done.wait();

    std::cout << "Goodbye!\n";
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
}