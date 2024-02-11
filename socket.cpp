#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/fcntl.h>
#include <vector>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "socket.h"

struct addrinfo_deleter {
    void operator()(addrinfo* a) { freeaddrinfo(a); }
};

using AddrInfo = std::unique_ptr<addrinfo, addrinfo_deleter>;

static void throw_err(const char* fn, const char* msg) {
    std::ostringstream ss;
    ss << fn << ": " << msg;
    throw std::runtime_error(ss.str());
}

AddrInfo get_address_info(const char* ip, unsigned short port) {
    if (port < 1024) {
        throw std::invalid_argument("ports under 1024 are reserved");
    }

    char port_str[6]; // 65535 + '\0'
    (void)snprintf(port_str, sizeof port_str, "%hu", port);

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (ip == nullptr) {
        hints.ai_flags = AI_PASSIVE;
    }

    addrinfo* result;
    if (int status = getaddrinfo(ip, port_str, &hints, &result); status != 0) {
        throw_err("getaddrinfo", gai_strerror(status));
    }

    return AddrInfo{result};
}

static int yes = 1;

static int create_server_fd(unsigned short port) {
    const auto addr = get_address_info(nullptr, port);

    auto fd = -1;

    for (auto p = addr.get(); p != nullptr; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) {
            continue;
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            throw_err("setsockopt", strerror(errno));
        }

        if (bind(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
            continue;
        }

        break;
    }

    if (fd == -1) {
        throw std::runtime_error("server failed to bind to an address");
    }

    constexpr int backlog_size = 10; // could be customizable through constructor param
    if (listen(fd, backlog_size) == -1) {
        throw_err("listen", strerror(errno));
    }

    return fd;
}

static const void* get_in_addr(const sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((sockaddr_in*)sa)->sin_addr);
    }
    return &(((sockaddr_in6*)sa)->sin6_addr);
}

static void send_data(int fd, std::span<const std::byte> data) {
    for (int total = 0, left = data.size(); total < data.size();) {
        const auto n = send(fd, data.subspan(total).data(), left, 0);
        if (n == -1) {
            throw_err("send", strerror(errno));
        }

        total += n;
        left -= n;
    }
}

static bool recv_data(int fd, std::vector<std::byte>& res) {
    for (int total = 0, left = res.size(); total < res.size();) {
        const auto n = recv(fd, res.data() + total, left, 0);
        if (n == -1) {
            throw_err("recv", strerror(errno));
        } else if (n == 0) {
            return false;
        }

        total += n;
        left -= n;
    }
    return true;
}

//
// ServerClient
//

struct ServerClient::Private {
    int fd;
    ServerClient::ID id;
    sockaddr_storage addr;
};

ServerClient::ServerClient(ServerClient::Private* p) : m(p) {}

ServerClient::ID ServerClient::id() const noexcept { return m->id; }

std::string ServerClient::address() const noexcept {
    char buf[INET6_ADDRSTRLEN];
    return inet_ntop(m->addr.ss_family, get_in_addr((sockaddr*)&m->addr), buf, sizeof buf);
}

void ServerClient::send(std::span<const std::byte> data) { send_data(m->fd, data); }

bool ServerClient::recv(std::vector<std::byte>& res) { return recv_data(m->fd, res); }

void ServerClient::set_blocking(bool should_block) {
    auto flags = fcntl(m->fd, F_GETFL, 0);
    if (flags == -1) {
        throw_err("fcntl", strerror(errno));
    }

    flags = should_block ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(m->fd, F_SETFL, flags) == -1) {
        throw_err("fcntl", strerror(errno));
    }
}

void ServerClient::close() {
    if (::close(m->fd) == -1) {
        throw_err("close", strerror(errno));
    }
    m->fd = -1;
}

ServerClient::~ServerClient() {
    try {
        if (m.use_count() == 1 && m->fd != -1) {
            close();
        }
    } catch (const std::exception& e) {
        (void)e;
    }
}

//
// Server
//

struct Server::Private {
    int fd;
    std::size_t next_id;
    std::vector<pollfd> pfd_buf;
};

Server::Server(unsigned short port)
    : m(new Server::Private{.fd = create_server_fd(port), .next_id = 0}) {}

static int accept_client_fd(int server_fd, sockaddr_storage* addr) {
    socklen_t sz = sizeof *addr;
    auto fd = accept(server_fd, (sockaddr*)addr, &sz);
    if (fd == -1) {
        throw_err("accept", strerror(errno));
    }
    return fd;
}

void Server::poll(std::span<const ServerClient> to_poll, std::vector<ServerPollResult>& res) {
    res.resize(0);
    m->pfd_buf.resize(0);

    m->pfd_buf.push_back(pollfd{.fd = m->fd, .events = POLLIN});
    for (const auto& c : to_poll) {
        m->pfd_buf.push_back(pollfd{.fd = c.m->fd, .events = POLLIN});
    }

    auto num_ready = ::poll(m->pfd_buf.data(), m->pfd_buf.size(), -1);
    if (num_ready == -1) {
        throw_err("poll", strerror(errno));
    }

    for (const auto& p : m->pfd_buf) {
        if (!(p.revents & POLLIN)) {
            continue;
        }

        if (p.fd == m->fd) {
            sockaddr_storage addr;
            const auto fd = accept_client_fd(m->fd, &addr);
            ServerClient c{new ServerClient::Private{.fd = fd, .id = ++m->next_id, .addr = addr}};
            ServerPollResult res_one{.client = c, .status = ServerClientStatus::New};
            res.push_back(res_one);
        } else {
            const auto it =
                std::find_if(to_poll.begin(), to_poll.end(), [&p](const ServerClient& c) {
                    return c.m->fd == p.fd;
                });
            ServerPollResult res_one{.client = *it, .status = ServerClientStatus::PendingData};
            res.push_back(res_one);
        }

        if (--num_ready == 0) {
            break;
        }
    }
}

void Server::shutdown() {
    if (::shutdown(m->fd, 2 /* further sends and recvs are disallowed */) == -1) {
        throw_err("shutdown", strerror(errno));
    }
    m->fd = -1;
}

Server::~Server() {
    try {
        if (m->fd != -1) {
            shutdown();
        }
    } catch (const std::exception& e) {
        (void)e;
    }
}

//
// Client
//

Client::Client(std::string ip, unsigned short port) : m_fd(-1) {
    // Would have used string_view but string_view doesn't have any way
    // to return a null byte terminated string. Probably could have used
    // const char* but not seeing raw strings like this makes me feel comfortable.
    const auto addr = get_address_info(ip.c_str(), port);

    for (auto p = addr.get(); p != nullptr; p = p->ai_next) {
        m_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_fd == -1) {
            continue;
        }

        if (connect(m_fd, p->ai_addr, p->ai_addrlen) == -1) {
            ::close(m_fd);
            continue;
        }

        break;
    }

    if (m_fd == -1) {
        throw std::runtime_error("client failed to connect to an address");
    }
}

void Client::send(std::span<const std::byte> data) { send_data(m_fd, data); }

bool Client::recv(std::vector<std::byte>& res) { return recv_data(m_fd, res); }

void Client::close() {
    if (::close(m_fd) == -1) {
        throw_err("close", strerror(errno));
    }
    m_fd = -1;
}

Client::~Client() {
    try {
        if (m_fd != -1) {
            close();
        }
    } catch (const std::exception& e) {
        (void)e;
    }
}