#pragma once

#include <cstdint>
#include <functional>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fritzmonitor {

struct SocketAddress {
  sockaddr_storage storage{};
  socklen_t length = 0;
  int socket_type = SOCK_STREAM;
  int protocol = 0;
  std::string numeric_host;
};

using AddressResolver =
    std::function<std::vector<SocketAddress>(const std::string&, std::uint16_t)>;

class TargetResolutionError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class UntrustedTargetError : public TargetResolutionError {
 public:
  using TargetResolutionError::TargetResolutionError;
};

struct ResolvedTarget {
  std::string host;
  std::uint16_t port = 0;
  std::vector<SocketAddress> addresses;
};

struct ResolvedHttpUrl {
  std::string url;
  std::string scheme;
  ResolvedTarget target;
};

std::vector<SocketAddress> system_resolve(const std::string& host,
                                          std::uint16_t port);
SocketAddress numeric_socket_address(const std::string& address,
                                     std::uint16_t port);
bool is_trusted_local_address(const sockaddr* address);
ResolvedTarget resolve_trusted_target(const std::string& host,
                                      std::uint16_t port,
                                      bool allow_nonlocal_addresses,
                                      const AddressResolver& resolver =
                                          system_resolve);
ResolvedHttpUrl resolve_trusted_http_url(
    const std::string& url, bool allow_nonlocal_addresses,
    const AddressResolver& resolver = system_resolve);
std::string curl_resolve_entry(const ResolvedTarget& target);
std::string http_url(const std::string& host, std::uint16_t port,
                     std::string_view path);

class RetryBackoff {
 public:
  RetryBackoff(int initial_seconds, int maximum_seconds);
  int next_delay_seconds();
  void reset();

 private:
  int initial_seconds_;
  int maximum_seconds_;
  int next_seconds_;
};

class FailureLogState {
 public:
  bool begin_failure();
  void recovered();

 private:
  bool failing_ = false;
};

}  // namespace fritzmonitor
