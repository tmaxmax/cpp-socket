#include <cstddef>
#include <exception>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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
    std::vector<std::byte> in, out;

    while (true) {
        server.poll(clients);
        for (auto& [client, _] : clients) {
            in.resize(8);
            if (!client.recv(in)) {
                server.remove(client);
                continue;
            }
            in.resize(parse_len(in));
            if (!client.recv(in)) {
                server.remove(client);
                continue;
            }
            std::cout << "Received: " << as_string(in) << '\n';
            pack_string(as_string(in), out);
            client.send(out);
        }
    }
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
}