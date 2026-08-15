#include "phonebook.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <curl/curl.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

class HttpServer {
 public:
  explicit HttpServer(bool challenge) : challenge_(challenge) {
    descriptor_ = socket(AF_INET, SOCK_STREAM, 0);
    assert(descriptor_ >= 0);
    const int enabled = 1;
    setsockopt(descriptor_, SOL_SOCKET, SO_REUSEADDR, &enabled,
               sizeof(enabled));
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(descriptor_, reinterpret_cast<sockaddr*>(&local),
                sizeof(local)) == 0);
    assert(listen(descriptor_, 4) == 0);
    socklen_t length = sizeof(local);
    assert(getsockname(descriptor_, reinterpret_cast<sockaddr*>(&local),
                       &length) == 0);
    port_ = ntohs(local.sin_port);
    thread_ = std::thread([this] { serve(); });
  }

  ~HttpServer() {
    if (thread_.joinable()) thread_.join();
    close(descriptor_);
  }

  std::uint16_t port() const { return port_; }

  std::vector<std::string> finish() {
    if (thread_.joinable()) thread_.join();
    return requests_;
  }

 private:
  void serve() {
    for (int attempt = 0; attempt < 2; ++attempt) {
      pollfd pending{descriptor_, POLLIN, 0};
      if (poll(&pending, 1, 1500) <= 0) break;
      const int client = accept(descriptor_, nullptr, nullptr);
      if (client < 0) break;
      std::string request;
      char buffer[2048];
      while (request.find("\r\n\r\n") == std::string::npos) {
        const auto count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        request.append(buffer, static_cast<std::size_t>(count));
      }
      requests_.push_back(request);
      const bool authorized =
          request.find("\r\nAuthorization:") != std::string::npos;
      const std::string response =
          challenge_ && !authorized
              ? "HTTP/1.1 401 Unauthorized\r\n"
                "WWW-Authenticate: Basic realm=\"test\"\r\n"
                "Content-Length: 0\r\nConnection: close\r\n\r\n"
              : "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n"
                "Connection: close\r\n\r\nOK";
      send(client, response.data(), response.size(), MSG_NOSIGNAL);
      close(client);
      if (!challenge_ || authorized) break;
    }
  }

  bool challenge_;
  int descriptor_ = -1;
  std::uint16_t port_ = 0;
  std::thread thread_;
  std::vector<std::string> requests_;
};

fritzmonitor::AddressResolver loopback_resolver() {
  return [](const std::string&, std::uint16_t port) {
    return std::vector<fritzmonitor::SocketAddress>{
        fritzmonitor::numeric_socket_address("127.0.0.1", port)};
  };
}

bool has_authorization(const std::string& request) {
  return request.find("\r\nAuthorization:") != std::string::npos;
}

}  // namespace

int main() {
  using namespace fritzmonitor;
  assert(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);

  Config config;
  config.host = "router.invalid";
  config.tr064_username = "alice";
  config.tr064_password = "secret";

  {
    HttpServer server(false);
    const auto plan = prepare_phonebook_download(
        "http://router.invalid:" + std::to_string(server.port()) +
            "/phonebook.lua",
        config, loopback_resolver());
    std::string response;
    assert(perform_phonebook_request(plan, config, nullptr, "", response));
    assert(response == "OK");
    const auto requests = server.finish();
    assert(requests.size() == 1);
    assert(!has_authorization(requests.front()));
  }

  {
    HttpServer server(true);
    const auto plan = prepare_phonebook_download(
        "http://router.invalid:" + std::to_string(server.port()) +
            "/phonebook.lua",
        config, loopback_resolver());
    std::string response;
    assert(!perform_phonebook_request(plan, config, nullptr, "", response));
    const auto requests = server.finish();
    assert(requests.size() == 1);
    assert(!has_authorization(requests.front()));
  }

  {
    HttpServer server(true);
    config.tr064_port = server.port();
    const auto plan = prepare_tr064_request(config, loopback_resolver());
    const std::string body = "<soap/>";
    std::string response;
    assert(perform_phonebook_request(plan, config, &body, "GetPhonebook",
                                     response));
    const auto requests = server.finish();
    assert(requests.size() == 2);
    assert(!has_authorization(requests.front()));
    assert(has_authorization(requests.back()));
  }

  curl_global_cleanup();
}
