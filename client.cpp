#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
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
    std::vector<std::byte> buf;

    for (std::string s; std::getline(std::cin, s);) {
        proto::pack(s, buf);
        client.send(buf);

        buf.resize(proto::size_header);
        if (!client.recv(buf)) {
            std::cout << "Server closed.\n";
            return 0;
        }

        const auto maybe_len = proto::unpack_header(buf);
        if (!maybe_len.has_value()) {
            std::cout << "Server sent invalid data.\n> ";
            continue;
        }

        buf.resize(*maybe_len);
        if (!client.recv(buf)) {
            std::cout << "Server closed.\n";
            return 0;
        }

        const auto maybe_output = proto::unpack_header(buf);
        if (!maybe_output.has_value()) {
            std::cout << "Server sent invalid data.\n> ";
            continue;
        }

        std::cout << *maybe_output;
    }

    proto::pack("", buf); // disconnect message is empty string
    client.send(buf);

    std::cout << "Goodbye!\n";

} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
}