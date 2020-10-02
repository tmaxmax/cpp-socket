#include "client.hxx"

#include <stdexcept>

Client::Client()
    : m_client() {
    if (!m_client.Initialize()) {
        throw std::runtime_error(m_client.DescribeError());
    }
}

auto Client::Connect(const char* addr, std::uint16_t port) -> void {
    if (!m_client.Open(addr, port)) {
        throw std::runtime_error(m_client.DescribeError());
    }
}

auto Client::SendMessage(std::string_view message) -> std::int32_t {
    auto bytes_sent = m_client.Send(reinterpret_cast<const std::uint8_t*>(message.data()), message.size());
    if (bytes_sent < 0) {
        throw std::runtime_error(m_client.DescribeError());
    }
    return bytes_sent;
}

auto Client::ReceiveMessage() -> std::string {
    auto bytes_recv = m_client.Receive(4096);
    if (bytes_recv < 0) {
        throw std::runtime_error(m_client.DescribeError());
    }
    std::string recv_msg(reinterpret_cast<const char*>(m_client.GetData()));
    recv_msg.resize(bytes_recv);
    if (recv_msg == "\n") {
        recv_msg.clear();
    }
    return std::move(recv_msg);
}

auto Client::Disconnect() -> void {
    if (!m_client.Shutdown(CActiveSocket::CShutdownMode::Both)) {
        throw std::runtime_error(m_client.DescribeError());
    }
}

Client::~Client() {
    m_client.Close();
}
