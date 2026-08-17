#include "callmonitor_client.hpp"

#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace fritzmonitor {
namespace {

bool connect_with_timeout(int descriptor, const SocketAddress& address) {
  const int flags = fcntl(descriptor, F_GETFL, 0);
  if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) != 0)
    return false;
  const auto* socket_address =
      reinterpret_cast<const sockaddr*>(&address.storage);
  if (connect(descriptor, socket_address, address.length) == 0) {
    fcntl(descriptor, F_SETFL, flags);
    return true;
  }
  if (errno != EINPROGRESS) return false;
  pollfd pending{descriptor, POLLOUT, 0};
  int status;
  do {
    status = poll(&pending, 1, 3000);
  } while (status < 0 && errno == EINTR);
  if (status <= 0) return false;
  int socket_error = 0;
  socklen_t error_length = sizeof(socket_error);
  if (getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &socket_error,
                 &error_length) != 0 ||
      socket_error != 0)
    return false;
  return fcntl(descriptor, F_SETFL, flags) == 0;
}

}  // namespace

CallmonitorClient::CallmonitorClient(Config config, EventCallback callback,
                                     AddressResolver resolver,
                                     ConnectionCallback connection_callback)
    : config_(std::move(config)),
      callback_(std::move(callback)),
      resolver_(std::move(resolver)),
      connection_callback_(std::move(connection_callback)) {}

int CallmonitorClient::run() {
  RetryBackoff backoff(config_.reconnect_seconds,
                       config_.reconnect_max_seconds);
  FailureLogState failure_log;
  if (connection_callback_) connection_callback_(false);
  for (;;) {
    ResolvedTarget target;
    std::string failure;
    try {
      target = resolve_trusted_target(
          config_.host, static_cast<std::uint16_t>(config_.port),
          config_.allow_nonlocal_addresses, resolver_);
    } catch (const TargetResolutionError& error) {
      failure = error.what();
    }

    int socket_fd = -1;
    if (failure.empty()) {
      for (const auto& address : target.addresses) {
        const auto* raw = reinterpret_cast<const sockaddr*>(&address.storage);
        socket_fd = socket(raw->sa_family, address.socket_type, address.protocol);
        if (socket_fd >= 0 && connect_with_timeout(socket_fd, address)) break;
        if (socket_fd >= 0) close(socket_fd);
        socket_fd = -1;
      }
      if (socket_fd < 0) failure = "connection failed";
    }

    if (socket_fd < 0) {
      const int delay = backoff.next_delay_seconds();
      if (failure_log.begin_failure())
        std::cerr << "fritzmonitor: FRITZ!Box unavailable (" << failure
                  << "); retrying with backoff up to "
                  << config_.reconnect_max_seconds << " seconds\n";
      std::this_thread::sleep_for(std::chrono::seconds(delay));
      continue;
    }

    failure_log.recovered();
    backoff.reset();
    std::cerr << "connected to " << config_.host << ':' << config_.port << '\n';
    if (connection_callback_) connection_callback_(true);
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
    if (connection_callback_) connection_callback_(false);
    std::cerr << "FRITZ!Box connection closed; retrying\n";
    std::this_thread::sleep_for(
        std::chrono::seconds(backoff.next_delay_seconds()));
  }
}

}  // namespace fritzmonitor
