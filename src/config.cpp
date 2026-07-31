#include "config.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fritzmonitor {
namespace {

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string value_of(const std::string& raw) {
  auto value = trim(raw);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    return value.substr(1, value.size() - 2);
  return value;
}

bool boolean_value(const std::string& value) { return value_of(value) == "true"; }

}  // namespace

std::filesystem::path default_config_path() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg) return std::filesystem::path(xdg) / "fritzmonitor/config.toml";
  const char* home = std::getenv("HOME");
  if (home && *home) return std::filesystem::path(home) / ".config/fritzmonitor/config.toml";
  return "config.toml";
}

Config load_config(const std::filesystem::path& path) {
  Config config;
  std::ifstream input(path);
  if (!input) return config;

  std::string line;
  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#') continue;
    const auto equals = line.find('=');
    if (equals == std::string::npos) continue;
    const auto key = trim(line.substr(0, equals));
    const auto value = trim(line.substr(equals + 1));
    try {
      if (key == "host") config.host = value_of(value);
      else if (key == "port") config.port = std::stoi(value_of(value));
      else if (key == "reconnect_seconds") config.reconnect_seconds = std::stoi(value_of(value));
      else if (key == "max_events") config.max_events = std::stoi(value_of(value));
      else if (key == "notify_incoming") config.notify_incoming = boolean_value(value);
      else if (key == "notify_missed") config.notify_missed = boolean_value(value);
      else if (key == "addressbook_enabled") config.addressbook_enabled = boolean_value(value);
      else if (key == "tr064_port") config.tr064_port = std::stoi(value_of(value));
      else if (key == "tr064_username") config.tr064_username = value_of(value);
      else if (key == "tr064_password") {
        config.tr064_password = value_of(value);
        config.tr064_password_from_config = !config.tr064_password.empty();
      }
    } catch (const std::exception&) {
      throw std::runtime_error("invalid value for config key: " + key);
    }
  }
  if (config.port < 1 || config.port > 65535) throw std::runtime_error("port out of range");
  if (config.reconnect_seconds < 1) throw std::runtime_error("reconnect_seconds must be positive");
  if (config.max_events < 1) throw std::runtime_error("max_events must be positive");
  if (config.tr064_port < 1 || config.tr064_port > 65535) throw std::runtime_error("tr064_port out of range");
  if (const char* username = std::getenv("FRITZMONITOR_TR064_USERNAME"); username && config.tr064_username.empty())
    config.tr064_username = username;
  if (const char* password = std::getenv("FRITZMONITOR_TR064_PASSWORD"); password && config.tr064_password.empty())
    config.tr064_password = password;
  return config;
}

void remove_tr064_password_from_config(const std::filesystem::path& path) {
  struct stat metadata {};
  if (::lstat(path.c_str(), &metadata) != 0)
    throw std::runtime_error("cannot inspect configuration: " + std::string(std::strerror(errno)));
  if (!S_ISREG(metadata.st_mode))
    throw std::runtime_error("configuration must be a regular file for credential migration");

  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open configuration for credential migration");
  const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (input.bad()) throw std::runtime_error("cannot read configuration for credential migration");

  std::string sanitized;
  bool removed = false;
  std::size_t position = 0;
  while (position < contents.size()) {
    const auto newline = contents.find('\n', position);
    const auto next = newline == std::string::npos ? contents.size() : newline + 1;
    auto line = contents.substr(position, next - position);
    auto inspected = trim(line);
    const auto equals = inspected.find('=');
    if (equals != std::string::npos && trim(inspected.substr(0, equals)) == "tr064_password") {
      removed = true;
    } else {
      sanitized.append(line);
    }
    position = next;
  }
  if (!removed) throw std::runtime_error("configuration does not contain tr064_password");

  auto pattern = (path.parent_path() / (path.filename().string() + ".tmp.XXXXXX")).string();
  std::vector<char> temporary_path(pattern.begin(), pattern.end());
  temporary_path.push_back('\0');
  const int descriptor = ::mkstemp(temporary_path.data());
  if (descriptor < 0)
    throw std::runtime_error("cannot create temporary configuration: " + std::string(std::strerror(errno)));

  const std::filesystem::path temporary(temporary_path.data());
  auto fail = [&](const std::string& message) {
    const int saved_errno = errno;
    ::close(descriptor);
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::runtime_error(message + ": " + std::string(std::strerror(saved_errno)));
  };
  if (::fchmod(descriptor, metadata.st_mode & 0777) != 0) fail("cannot set configuration permissions");

  std::size_t written = 0;
  while (written < sanitized.size()) {
    const auto result = ::write(descriptor, sanitized.data() + written, sanitized.size() - written);
    if (result < 0) {
      if (errno == EINTR) continue;
      fail("cannot write migrated configuration");
    }
    written += static_cast<std::size_t>(result);
  }
  if (::fsync(descriptor) != 0) fail("cannot synchronize migrated configuration");
  if (::close(descriptor) != 0) {
    const int saved_errno = errno;
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::runtime_error("cannot close migrated configuration: " + std::string(std::strerror(saved_errno)));
  }
  if (::rename(temporary.c_str(), path.c_str()) != 0) {
    const int saved_errno = errno;
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw std::runtime_error("cannot replace configuration: " + std::string(std::strerror(saved_errno)));
  }
}

}  // namespace fritzmonitor
