#include "callmonitor_client.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace fritzmonitor {

CallmonitorClient::CallmonitorClient(Config config, EventCallback callback)
    : config_(std::move(config)), callback_(std::move(callback)) {}

int CallmonitorClient::run() {
  for (;;) {
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo* result = nullptr;
    const auto port = std::to_string(config_.port);
    const int lookup = getaddrinfo(config_.host.c_str(), port.c_str(), &hints, &result);
    if (lookup != 0) {
      std::cerr << "FRITZ!Box lookup failed: " << gai_strerror(lookup) << '\n';
      std::this_thread::sleep_for(std::chrono::seconds(config_.reconnect_seconds));
      continue;
    }

    int socket_fd = -1;
    for (auto* address = result; address; address = address->ai_next) {
      socket_fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
      if (socket_fd >= 0 && connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) break;
      if (socket_fd >= 0) close(socket_fd);
      socket_fd = -1;
    }
    freeaddrinfo(result);
    if (socket_fd < 0) {
      std::cerr << "FRITZ!Box connection failed; retrying\n";
      std::this_thread::sleep_for(std::chrono::seconds(config_.reconnect_seconds));
      continue;
    }

    std::cerr << "connected to " << config_.host << ':' << config_.port << '\n';
    std::string buffer;
    char chunk[1024];
    while (true) {
      const auto count = recv(socket_fd, chunk, sizeof(chunk), 0);
      if (count <= 0) break;
      buffer.append(chunk, static_cast<std::size_t>(count));
      std::size_t newline;
      while ((newline = buffer.find('\n')) != std::string::npos) {
        auto line = buffer.substr(0, newline);
        buffer.erase(0, newline + 1);
        if (auto event = parse_callmonitor_line(line)) callback_(*event);
      }
    }
    close(socket_fd);
    std::cerr << "FRITZ!Box connection closed; retrying\n";
    std::this_thread::sleep_for(std::chrono::seconds(config_.reconnect_seconds));
  }
}

}  // namespace fritzmonitor
