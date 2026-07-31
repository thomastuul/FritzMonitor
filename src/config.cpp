#include "config.hpp"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>

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
      else if (key == "tr064_password") config.tr064_password = value_of(value);
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

}  // namespace fritzmonitor
