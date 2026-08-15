#include "network_target.hpp"
#include "phonebook.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using fritzmonitor::SocketAddress;

SocketAddress address(const std::string& value) {
  return fritzmonitor::numeric_socket_address(value, 1012);
}

void expect_untrusted(const std::string& value) {
  const fritzmonitor::AddressResolver resolver =
      [value](const std::string&, std::uint16_t) {
        return std::vector<SocketAddress>{address(value)};
      };
  bool rejected = false;
  try {
    fritzmonitor::resolve_trusted_target("router.test", 1012, false,
                                        resolver);
  } catch (const fritzmonitor::UntrustedTargetError&) {
    rejected = true;
  }
  assert(rejected);
}

}  // namespace

int main() {
  using namespace fritzmonitor;

  for (const auto* value : {"10.23.4.5", "172.16.0.1", "172.31.255.254",
                            "192.168.178.1", "169.254.7.8", "127.0.0.1",
                            "fc00::1", "fd12:3456::1", "fe80::1", "::1",
                            "::ffff:192.168.1.1"}) {
    const auto candidate = address(value);
    assert(is_trusted_local_address(
        reinterpret_cast<const sockaddr*>(&candidate.storage)));
  }

  for (const auto* value : {"8.8.8.8", "212.42.244.122", "100.64.0.1",
                            "224.0.0.1", "2001:4860:4860::8888",
                            "2001:bf0:244:244::122", "ff02::1", "::"}) {
    expect_untrusted(value);
  }

  const AddressResolver mixed = [](const std::string&, std::uint16_t) {
    return std::vector<SocketAddress>{address("192.168.178.1"),
                                      address("212.42.244.122")};
  };
  bool mixed_rejected = false;
  try {
    resolve_trusted_target("fritz.box", 1012, false, mixed);
  } catch (const UntrustedTargetError&) {
    mixed_rejected = true;
  }
  assert(mixed_rejected);
  assert(resolve_trusted_target("remote.example", 1012, true, mixed)
             .addresses.size() == 2);

  int resolver_calls = 0;
  const AddressResolver changing = [&resolver_calls](const std::string&,
                                                     std::uint16_t) {
    ++resolver_calls;
    return std::vector<SocketAddress>{
        address(resolver_calls == 1 ? "192.168.178.1" : "212.42.244.122")};
  };
  const auto pinned = resolve_trusted_target("fritz.box", 49000, false,
                                             changing);
  const auto resolve_entry = curl_resolve_entry(pinned);
  assert(resolver_calls == 1);
  assert(resolve_entry == "fritz.box:49000:192.168.178.1");
  assert(resolve_entry.find("212.42.244.122") == std::string::npos);
  const ResolvedTarget ipv6_target{
      "fd12:3456::1", 49000, {numeric_socket_address("fd12:3456::1", 49000)}};
  assert(curl_resolve_entry(ipv6_target) ==
         "[fd12:3456::1]:49000:[fd12:3456::1]");

  const AddressResolver local = [](const std::string&, std::uint16_t port) {
    return std::vector<SocketAddress>{
        numeric_socket_address("192.168.178.1", port)};
  };
  Config config;
  config.addressbook_enabled = true;
  config.tr064_username = "user";
  config.tr064_password = "secret";
  const auto tr064 = prepare_tr064_request(config, local);
  assert(tr064.send_tr064_credentials);
  assert(tr064.target.target.port == 49000);
  const auto download = prepare_phonebook_download(
      "http://fritz.box:49000/phonebook.lua?sid=test", config, local);
  assert(!download.send_tr064_credentials);

  bool public_download_rejected = false;
  const AddressResolver public_resolver =
      [](const std::string&, std::uint16_t port) {
        return std::vector<SocketAddress>{
            numeric_socket_address("212.42.244.122", port)};
      };
  try {
    prepare_phonebook_download("http://evil.example/book", config,
                               public_resolver);
  } catch (const UntrustedTargetError&) {
    public_download_rejected = true;
  }
  assert(public_download_rejected);

  bool mixed_download_rejected = false;
  try {
    prepare_phonebook_download("http://mixed.example/book", config, mixed);
  } catch (const UntrustedTargetError&) {
    mixed_download_rejected = true;
  }
  assert(mixed_download_rejected);

  bool unsafe_scheme_rejected = false;
  try {
    prepare_phonebook_download("file:///etc/passwd", config, local);
  } catch (const TargetResolutionError&) {
    unsafe_scheme_rejected = true;
  }
  assert(unsafe_scheme_rejected);

  bool resolve_directive_rejected = false;
  try {
    prepare_phonebook_download("http://-router.example/book", config, local);
  } catch (const TargetResolutionError&) {
    resolve_directive_rejected = true;
  }
  assert(resolve_directive_rejected);

  RetryBackoff backoff(5, 60);
  for (const int expected : {5, 10, 20, 40, 60, 60})
    assert(backoff.next_delay_seconds() == expected);
  backoff.reset();
  assert(backoff.next_delay_seconds() == 5);

  FailureLogState logs;
  assert(logs.begin_failure());
  assert(!logs.begin_failure());
  logs.recovered();
  assert(logs.begin_failure());
}
