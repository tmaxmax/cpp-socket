#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "protocol.h"
#include "receive.h"
#include "socket.h"

class Username {
private:
    std::string value;

    explicit Username(std::string_view s) : value(s) {}

public:
    static std::optional<Username> parse(std::string_view s) {
        // User names should be of the form [a-z0-9-_]{3,30}.
        if (s.size() < 3 || s.size() > 30) {
            return std::nullopt;
        }
        const auto pos_invalid = std::find_if(s.begin(), s.end(), [](int c) {
            return !(islower(c) || isdigit(c) || c == '-' || c == '_');
        });
        if (pos_invalid != s.end()) {
            return std::nullopt;
        }
        // User name also can't be "bc" but that case is handled by the length check.
        return Username(s);
    }

    operator std::string_view() const noexcept { return value; }
};

class Registry {
private:
    // INVARIANTS:
    // 1. All information in id_to_user_name and user_name_to_client must correspond to a client
    // inside m_clients.
    // 2. user_name_to_client contains views of Usernames inside id_to_user_name. Handle with care
    // to not reference invalid memory.
    // 3. Unregistered clients do not have any information in id_to_user_name or
    // user_name_to_client.
    //
    // Note: we do linear searches on m_clients. This should not be a performance issue for our
    // use case, given that it is not expected to have a lot of clients, and makes working with
    // the server abstraction easier and more performant – if we were to store it in a map,
    // we'd have to construct a vector for each call to poll().

    std::vector<ServerClient> m_clients;
    std::unordered_map<ServerClient::ID, Username> id_to_user_name;
    std::unordered_map<std::string_view, ServerClient> user_name_to_client;

    auto find_by_id(ServerClient::ID id) const noexcept {
        return std::find_if(m_clients.begin(), m_clients.end(), [id](const ServerClient& c) {
            return c.id() == id;
        });
    }

public:
    void add_unregistered(ServerClient client) {
        const auto it = find_by_id(client.id());
        if (it != m_clients.end()) {
            throw std::logic_error("tried to add already added client");
        }

        m_clients.push_back(client);
    }

    bool is_registered(ServerClient::ID id) { return id_to_user_name.contains(id); }

    std::optional<std::reference_wrapper<Username>> get_user_name(ServerClient::ID id) {
        const auto it = id_to_user_name.find(id);
        if (it == id_to_user_name.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<ServerClient> get_client(const Username& user_name) {
        const auto it = user_name_to_client.find(user_name);
        if (it == user_name_to_client.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool register_client(ServerClient::ID id, Username user_name) {
        if (user_name_to_client.contains(user_name)) {
            return false;
        }

        const auto it = find_by_id(id);
        if (it == m_clients.end() || is_registered(it->id())) {
            throw std::logic_error("tried to register inexistent or already registered client");
        }

        id_to_user_name.emplace(std::make_pair(id, std::move(user_name)));
        user_name_to_client[id_to_user_name.at(id)] = *it;

        return true;
    }

    void remove(ServerClient::ID id) {
        const auto it = find_by_id(id);
        if (it == m_clients.end()) {
            throw std::logic_error("tried to remove inexistent client");
        }

        const auto c = *it;

        m_clients.erase(it);

        if (!is_registered(id)) {
            return;
        }

        const auto& user_name = id_to_user_name.at(id);
        user_name_to_client.erase(user_name);
        id_to_user_name.erase(id);
    }

    std::span<ServerClient> clients() noexcept { return m_clients; }
};

static void send_to_all_registered_except(
    Registry&, ServerClient::ID, std::string_view, std::vector<std::byte>&);

static void remove_and_broadcast(
    ServerClient::ID to_remove, Registry& reg, bool is_unexpected, std::vector<std::byte>& buf) {
    const auto user_name = reg.get_user_name(to_remove);
    if (!user_name.has_value()) {
        // No need to announce if the client was not registered, as no clients can communicate with
        // it.
        return;
    }

    std::ostringstream out;
    out << "\n"
        << std::string_view(user_name->get()) << " has "
        << (is_unexpected ? "been disconnected" : "left") << ".\n> ";

    send_to_all_registered_except(reg, to_remove, out.str(), buf);

    reg.remove(to_remove);
}

static bool
send_or_remove(ServerClient& c, Registry& reg, std::string_view msg, std::vector<std::byte>& buf) {
    buf.resize(0);
    proto::pack(msg, buf);

    try {
        c.send(buf);
        return true;
    } catch (const SocketError&) {
        remove_and_broadcast(c.id(), reg, true, buf);
        return false;
    }
}

static void send_to_all_registered_except(
    Registry& reg, ServerClient::ID omit, std::string_view msg, std::vector<std::byte>& buf) {
    buf.resize(0);
    proto::pack(msg, buf);

    std::vector<ServerClient::ID> failed;
    for (auto& client : reg.clients()) {
        if (client.id() == omit || !reg.is_registered(client.id())) {
            continue;
        }

        try {
            client.send(buf);
        } catch (const SocketError&) {
            failed.push_back(client.id());
        }
    }

    for (auto id : failed) {
        remove_and_broadcast(id, reg, true, buf);
    }
}

class ServerClientNonBlockGuard {
private:
    ServerClient client;

public:
    ServerClientNonBlockGuard(ServerClient& c) : client(c) { client.set_blocking(false); }
    ~ServerClientNonBlockGuard() { client.set_blocking(true); }
};

static std::optional<std::string>
recv_or_remove(ServerClient& client, Registry& reg, std::vector<std::byte>& buf) try {
    ReceiveResult recv;
    {
        ServerClientNonBlockGuard g(client);
        recv = receive(client, buf);
    }
    if (!recv.is_connected) {
        remove_and_broadcast(client.id(), reg, true, buf);
        return std::nullopt;
    }
    if (!recv.message.has_value()) {
        send_or_remove(client, reg, "I couldn't quite get that. Can you say it again?\n> ", buf);
        return std::nullopt;
    }
    if (recv.message == "") { // disconnect
        remove_and_broadcast(client.id(), reg, false, buf);
        return std::nullopt;
    }
    return std::move(recv.message);
} catch (const SocketError& e) {
    if (e.would_block()) {
        return std::nullopt;
    }
    throw e;
}

static void handle_new_client(ServerClient& client, Registry& reg, std::vector<std::byte>& buf) {
    reg.add_unregistered(client);

    send_or_remove(client, reg, "Hi there! Please give us your username.\n> ", buf);
}

static void
handle_unregistered_client_data(ServerClient& client, Registry& reg, std::vector<std::byte>& buf) {
    auto recv = recv_or_remove(client, reg, buf);
    if (!recv.has_value()) {
        return;
    }

    auto maybe_user_name = Username::parse(*recv);
    if (!maybe_user_name.has_value()) {
        send_or_remove(client, reg, "That's not a valid user name. Try again!\n> ", buf);
        return;
    }

    if (!reg.register_client(client.id(), std::move(*maybe_user_name))) {
        send_or_remove(client, reg, "This user name is taken. Try again!\n> ", buf);
        return;
    }

    std::ostringstream out;
    out << "Registered!\nCurrently active users:\n";

    for (const auto& c : reg.clients()) {
        const auto user_name = reg.get_user_name(c.id());
        if (!user_name.has_value()) {
            continue;
        }

        out << " - " << std::string_view(user_name->get());
        if (c.id() == client.id()) {
            out << " (you)";
        }
        out << '\n';
    }

    out << "To send a message to someone, type \"<username> <your message>\"\n"
           "To send a message to everyone, type \"bc <your message>\"\n"
           "Happy chatting!\n\n"
           "> ";

    if (!send_or_remove(client, reg, out.str(), buf)) {
        return;
    }

    out.str("");
    out << '\n' << std::string_view(reg.get_user_name(client.id())->get()) << " is here!\n> ";

    send_to_all_registered_except(reg, client.id(), out.str(), buf);
}

class indent {
private:
    std::string_view s;

public:
    explicit indent(std::string_view s) : s(s) {}
    friend std::ostream& operator<<(std::ostream& os, const indent& i) {
        std::string_view s = i.s;

        for (std::size_t pos_lf; (pos_lf = s.find('\n')) != std::string::npos;) {
            os << "  " << s.substr(0, pos_lf + 1);
            s = s.substr(pos_lf + 1);
        }

        return os << "  " << s;
    }
};

static void handle_broadcast(
    ServerClient& from, Registry& reg, std::string_view msg, std::vector<std::byte>& buf) {
    const auto user_name = reg.get_user_name(from.id());

    std::ostringstream out;
    out << '\n' << std::string_view(user_name->get()) << " to everyone:\n" << indent(msg) << "\n> ";

    send_to_all_registered_except(reg, from.id(), out.str(), buf);
    send_or_remove(from, reg, "> ", buf);
}

static void handle_private(
    ServerClient& from, ServerClient& to, Registry& reg, std::string_view msg,
    std::vector<std::byte>& buf) {
    const auto user_name = reg.get_user_name(from.id());

    std::ostringstream out("\n");
    if (from.id() == to.id()) {
        out << "Note to self:";
    } else {
        out << std::string_view(user_name->get()) << " to you:";
    }
    out << '\n' << indent(msg) << "\n> ";

    send_or_remove(to, reg, out.str(), buf);
    if (from.id() != to.id()) {
        send_or_remove(from, reg, "> ", buf);
    }
}

static void
handle_registered_client_data(ServerClient& client, Registry& reg, std::vector<std::byte>& buf) {
    auto recv = recv_or_remove(client, reg, buf);
    if (!recv.has_value()) {
        return;
    }

    const auto pos_blank = recv->find(' ');
    if (pos_blank == std::string::npos) {
        send_or_remove(client, reg, "Can't send empty message. Try again!\n> ", buf);
        return;
    }

    std::string_view user_name_in(recv->data(), pos_blank);
    std::string_view msg(recv->data() + pos_blank + 1, recv->size() - pos_blank - 1);

    if (user_name_in == "bc") {
        handle_broadcast(client, reg, msg, buf);
        return;
    }

    const auto maybe_user_name = Username::parse(user_name_in);
    if (!maybe_user_name.has_value()) {
        send_or_remove(client, reg, "Invalid user name. Try again!\n> ", buf);
        return;
    }

    auto maybe_to = reg.get_client(*maybe_user_name);
    if (!maybe_to.has_value()) {
        send_or_remove(client, reg, "This user doesn't exist. Misspelled?\n> ", buf);
        return;
    }

    handle_private(client, *maybe_to, reg, msg, buf);
}

int main(int argc, char** argv) try {
    if (argc < 2) {
        std::cerr << "termchat: no port specified\n";
        return 1;
    }

    const unsigned short port = std::stoul(argv[1]);

    Server server(port);
    Registry registry;
    std::vector<ServerPollResult> polled;
    std::vector<std::byte> buf;

    while (true) {
        server.poll(registry.clients(), polled);
        for (auto& [client, status] : polled) {
            switch (status) {
            case ServerClientStatus::New:
                handle_new_client(client, registry, buf);
            case ServerClientStatus::PendingData:
                if (registry.is_registered(client.id())) {
                    handle_registered_client_data(client, registry, buf);
                } else {
                    handle_unregistered_client_data(client, registry, buf);
                }
            }
        }
    }
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
}