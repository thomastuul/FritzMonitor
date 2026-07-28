#pragma once

#include <filesystem>
#include <string>

namespace fritzmonitor {

struct Config {
  std::string host = "fritz.box";
  int port = 1012;
  int reconnect_seconds = 5;
  int max_events = 20;
  bool notify_incoming = true;
  bool notify_missed = true;
};

Config load_config(const std::filesystem::path& path);
std::filesystem::path default_config_path();

}  // namespace fritzmonitor
