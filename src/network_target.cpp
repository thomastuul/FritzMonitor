#include "network_target.hpp"

#include <arpa/inet.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <netdb.h>
#include <set>

namespace fritzmonitor {
namespace {

bool trusted_ipv4(const in_addr& address) {
  const auto value = ntohl(address.s_addr);
  return (value & 0xff000000U) == 0x0a000000U ||
         (value & 0xfff00000U) == 0xac100000U ||
         (value & 0xffff0000U) == 0xc0a80000U ||
         (value & 0xffff0000U) == 0xa9fe0000U ||
         (value & 0xff000000U) == 0x7f000000U;
}

std::uint16_t parse_port(const std::string& value) {
  if (value.empty() ||
      !std::all_of(value.begin(), value.end(),
                   [](unsigned char c) { return std::isdigit(c); })) {
    throw TargetResolutionError("HTTP URL has an invalid port");
  }
  try {
    const auto port = std::stoul(value);
    if (port < 1 || port > 65535)
      throw TargetResolutionError("HTTP URL port is out of range");
    return static_cast<std::uint16_t>(port);
  } catch (const TargetResolutionError&) {
    throw;
  } catch (const std::exception&) {
    throw TargetResolutionError("HTTP URL has an invalid port");
  }
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

}  // namespace

std::vector<SocketAddress> system_resolve(const std::string& host,
                                          std::uint16_t port) {
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  addrinfo* result = nullptr;
  const auto service = std::to_string(port);
  const int status =
      getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
  if (status != 0)
    throw TargetResolutionError("lookup failed: " +
                                std::string(gai_strerror(status)));

  std::vector<SocketAddress> addresses;
  std::set<std::string> seen;
  for (auto* current = result; current; current = current->ai_next) {
    if (current->ai_family != AF_INET && current->ai_family != AF_INET6)
      continue;
    char numeric[NI_MAXHOST]{};
    if (getnameinfo(current->ai_addr, current->ai_addrlen, numeric,
                    sizeof(numeric), nullptr, 0, NI_NUMERICHOST) != 0)
      continue;
    const std::string key = std::to_string(current->ai_family) + ':' + numeric;
    if (!seen.insert(key).second) continue;
    SocketAddress address;
    std::memcpy(&address.storage, current->ai_addr, current->ai_addrlen);
    address.length = static_cast<socklen_t>(current->ai_addrlen);
    address.socket_type = current->ai_socktype;
    address.protocol = current->ai_protocol;
    address.numeric_host = numeric;
    addresses.push_back(std::move(address));
  }
  freeaddrinfo(result);
  if (addresses.empty())
    throw TargetResolutionError("lookup returned no IPv4 or IPv6 address");
  return addresses;
}

SocketAddress numeric_socket_address(const std::string& address,
                                     std::uint16_t port) {
  auto addresses = system_resolve(address, port);
  if (addresses.size() != 1)
    throw TargetResolutionError("numeric address was not unambiguous: " + address);
  return addresses.front();
}

bool is_trusted_local_address(const sockaddr* address) {
  if (!address) return false;
  if (address->sa_family == AF_INET) {
    return trusted_ipv4(reinterpret_cast<const sockaddr_in*>(address)->sin_addr);
  }
  if (address->sa_family != AF_INET6) return false;
  const auto& ipv6 = reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr;
  if (IN6_IS_ADDR_LOOPBACK(&ipv6) || IN6_IS_ADDR_LINKLOCAL(&ipv6)) return true;
  if ((ipv6.s6_addr[0] & 0xfeU) == 0xfcU) return true;
  if (IN6_IS_ADDR_V4MAPPED(&ipv6)) {
    in_addr mapped{};
    std::memcpy(&mapped, &ipv6.s6_addr[12], sizeof(mapped));
    return trusted_ipv4(mapped);
  }
  return false;
}

ResolvedTarget resolve_trusted_target(const std::string& host,
                                      std::uint16_t port,
                                      bool allow_nonlocal_addresses,
                                      const AddressResolver& resolver) {
  auto addresses = resolver(host, port);
  if (addresses.empty())
    throw TargetResolutionError("lookup returned no IPv4 or IPv6 address");
  if (!allow_nonlocal_addresses) {
    for (const auto& address : addresses) {
      if (!is_trusted_local_address(
              reinterpret_cast<const sockaddr*>(&address.storage))) {
        throw UntrustedTargetError("untrusted address " + address.numeric_host);
      }
    }
  }
  return ResolvedTarget{host, port, std::move(addresses)};
}

ResolvedHttpUrl resolve_trusted_http_url(const std::string& url,
                                         bool allow_nonlocal_addresses,
                                         const AddressResolver& resolver) {
  if (std::any_of(url.begin(), url.end(),
                  [](unsigned char c) { return std::iscntrl(c) || std::isspace(c); }))
    throw TargetResolutionError("HTTP URL contains whitespace or control characters");
  const auto scheme_end = url.find("://");
  if (scheme_end == std::string::npos)
    throw TargetResolutionError("HTTP URL has no explicit scheme");
  const auto scheme = lowercase(url.substr(0, scheme_end));
  if (scheme != "http" && scheme != "https")
    throw TargetResolutionError("HTTP URL scheme must be http or https");

  const auto authority_start = scheme_end + 3;
  const auto authority_end = url.find_first_of("/?#", authority_start);
  const auto authority = url.substr(
      authority_start, authority_end == std::string::npos
                           ? std::string::npos
                           : authority_end - authority_start);
  if (authority.empty() || authority.find('@') != std::string::npos)
    throw TargetResolutionError("HTTP URL has an invalid authority");

  std::string host;
  std::uint16_t port = scheme == "https" ? 443 : 80;
  if (authority.front() == '[') {
    const auto close = authority.find(']');
    if (close == std::string::npos)
      throw TargetResolutionError("HTTP URL has an invalid IPv6 host");
    host = authority.substr(1, close - 1);
    if (close + 1 < authority.size()) {
      if (authority[close + 1] != ':')
        throw TargetResolutionError("HTTP URL has an invalid authority");
      port = parse_port(authority.substr(close + 2));
    }
    for (std::size_t position = 0;
         (position = host.find("%25", position)) != std::string::npos;) {
      host.replace(position, 3, "%");
      ++position;
    }
  } else {
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
      if (authority.find(':') != colon)
        throw TargetResolutionError("IPv6 hosts in HTTP URLs must use brackets");
      host = authority.substr(0, colon);
      port = parse_port(authority.substr(colon + 1));
    } else {
      host = authority;
    }
  }
  if (host.empty()) throw TargetResolutionError("HTTP URL has no host");
  if (host == "*" || host.front() == '+' || host.front() == '-' ||
      !std::all_of(host.begin(), host.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-' || c == '_' ||
               c == ':' || c == '%';
      })) {
    throw TargetResolutionError("HTTP URL has an unsupported host syntax");
  }
  return ResolvedHttpUrl{
      url, scheme,
      resolve_trusted_target(host, port, allow_nonlocal_addresses, resolver)};
}

std::string curl_resolve_entry(const ResolvedTarget& target) {
  const auto resolve_host = target.host.find(':') == std::string::npos
                                ? target.host
                                : '[' + target.host + ']';
  std::string entry = resolve_host + ':' + std::to_string(target.port) + ':';
  for (std::size_t index = 0; index < target.addresses.size(); ++index) {
    if (index) entry.push_back(',');
    const auto& address = target.addresses[index];
    if (reinterpret_cast<const sockaddr*>(&address.storage)->sa_family == AF_INET6)
      entry += '[' + address.numeric_host + ']';
    else
      entry += address.numeric_host;
  }
  return entry;
}

std::string http_url(const std::string& host, std::uint16_t port,
                     std::string_view path) {
  std::string formatted_host = host;
  if (host.find(':') != std::string::npos) {
    for (std::size_t position = 0;
         (position = formatted_host.find('%', position)) != std::string::npos;) {
      formatted_host.replace(position, 1, "%25");
      position += 3;
    }
    formatted_host = '[' + formatted_host + ']';
  }
  return "http://" + formatted_host + ':' + std::to_string(port) +
         std::string(path);
}

RetryBackoff::RetryBackoff(int initial_seconds, int maximum_seconds)
    : initial_seconds_(initial_seconds),
      maximum_seconds_(maximum_seconds),
      next_seconds_(initial_seconds) {
  if (initial_seconds < 1 || maximum_seconds < initial_seconds)
    throw std::invalid_argument("invalid retry backoff interval");
}

int RetryBackoff::next_delay_seconds() {
  const int delay = next_seconds_;
  if (next_seconds_ < maximum_seconds_) {
    next_seconds_ = next_seconds_ > maximum_seconds_ / 2
                        ? maximum_seconds_
                        : std::min(maximum_seconds_, next_seconds_ * 2);
  }
  return delay;
}

void RetryBackoff::reset() { next_seconds_ = initial_seconds_; }

bool FailureLogState::begin_failure() {
  if (failing_) return false;
  failing_ = true;
  return true;
}

void FailureLogState::recovered() { failing_ = false; }

}  // namespace fritzmonitor
