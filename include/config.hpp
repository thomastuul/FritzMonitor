#pragma once

#include <filesystem>
#include <string>

namespace fritzmonitor {

struct Config {
  std::string host = "fritz.box";
  int port = 1012;
  int reconnect_seconds = 5;
  int reconnect_max_seconds = 60;
  bool allow_nonlocal_addresses = false;
  int max_events = 20;
  bool notify_incoming = true;
  bool notify_missed = true;
  bool addressbook_enabled = false;
  int tr064_port = 49000;
  std::string tr064_username;
  std::string tr064_password;
  bool tr064_password_from_config = false;
};

Config load_config(const std::filesystem::path& path);
std::filesystem::path default_config_path();
void remove_tr064_password_from_config(const std::filesystem::path& path);

}  // namespace fritzmonitor
