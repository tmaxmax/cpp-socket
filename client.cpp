#include <cstddef>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "protocol.h"
#include "socket.h"

int main(int argc, char** argv) try {
    if (argc < 3) {
        std::cerr << "termchat: ip and port must be specified\n";
        return 1;
    }

    const unsigned short port = std::stoul(argv[2]);

    Client client(argv[1], port);
    std::vector<std::byte> msg;

    for (std::string s; std::getline(std::cin, s);) {
        pack_string(s, msg);
        client.send(msg);

        std::vector<std::byte> data(sizeof s.size());
        if (!client.recv(data)) {
            break;
        }
        const auto len = parse_len(data);
        data.resize(len);
        if (!client.recv(data)) {
            break;
        }

        std::cout << as_string(data) << '\n';
    }

    std::cerr << "Done\n";

} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
}