#include "../client.hxx"
#include "../logger.hxx"

#include <iostream>
#include <string>
#include <future>

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            throw std::runtime_error("IP and Port arguments expected!");
        }
        auto port = std::stoi(argv[2]);
        if (port < 0 || port > 65535) {
            throw std::runtime_error("Port too big or too small!");
        }

        Client client;
        client.Connect(argv[1], port);

        auto receive_done = std::async(std::launch::async, [&]() {
            while (true) {
                const auto recv_msg = client.ReceiveMessage();
                if (recv_msg.empty()) {
                    return;
                }
                teo::Log("Message received: {}", recv_msg);
                if (recv_msg == "Server closed!") {
                    teo::Log("Server was closed, any future messages will not be sent!");
                    return;
                }
            }
        });

        using namespace std::chrono_literals;
        while (true) {
            std::string message;
            std::getline(std::cin, message);

            if (receive_done.wait_for(0ns) == std::future_status::ready) {
                break;
            }

            if (message == "disconnect") {
                break;
            }

            client.SendMessage(message);

            if (message == "close") {
                break;
            }
        }

        receive_done.wait();
        client.Disconnect();
        teo::Log("Communication with server ended!");
    } catch (const std::exception& e) {
        teo::Log(e.what());
    }
}
