#include "call_event.hpp"
#include "callmonitor_client.hpp"
#include "config.hpp"
#include "desktop.hpp"
#include "phonebook.hpp"
#include "secret_store.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <termios.h>
#include <unistd.h>

namespace {

void write_terminal(int descriptor, const std::string& value) {
  std::size_t written = 0;
  while (written < value.size()) {
    const auto result = ::write(descriptor, value.data() + written, value.size() - written);
    if (result < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("cannot write to terminal: " + std::string(std::strerror(errno)));
    }
    written += static_cast<std::size_t>(result);
  }
}

std::string read_password(const std::string& prompt) {
  const int descriptor = ::open("/dev/tty", O_RDWR | O_CLOEXEC);
  if (descriptor < 0)
    throw std::runtime_error("cannot open terminal for password input: " + std::string(std::strerror(errno)));

  struct TerminalGuard {
    int descriptor;
    termios original{};
    bool changed = false;
    ~TerminalGuard() {
      if (changed) ::tcsetattr(descriptor, TCSAFLUSH, &original);
      ::close(descriptor);
    }
  } guard{descriptor};

  if (::tcgetattr(descriptor, &guard.original) != 0)
    throw std::runtime_error("cannot inspect terminal settings: " + std::string(std::strerror(errno)));
  auto hidden = guard.original;
  hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
  write_terminal(descriptor, prompt);
  if (::tcsetattr(descriptor, TCSAFLUSH, &hidden) != 0)
    throw std::runtime_error("cannot hide terminal input: " + std::string(std::strerror(errno)));
  guard.changed = true;

  std::string password;
  char character = '\0';
  while (true) {
    const auto result = ::read(descriptor, &character, 1);
    if (result < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("cannot read password: " + std::string(std::strerror(errno)));
    }
    if (result == 0 || character == '\n' || character == '\r') break;
    password.push_back(character);
  }
  ::tcsetattr(descriptor, TCSAFLUSH, &guard.original);
  guard.changed = false;
  write_terminal(descriptor, "\n");
  return password;
}

void wipe(std::string& value) {
  volatile char* data = value.empty() ? nullptr : value.data();
  for (std::size_t index = 0; index < value.size(); ++index) data[index] = '\0';
  value.clear();
}

struct SensitiveStringGuard {
  std::string* value;
  ~SensitiveStringGuard() {
    if (value) wipe(*value);
  }
};

}  // namespace

int main(int argc, char** argv) {
  using namespace fritzmonitor;
  try {
    auto config_path = default_config_path();
    bool simulate = false;
    bool store_password = false;
    bool migrate_password = false;
    for (int i = 1; i < argc; ++i) {
      const std::string argument = argv[i];
      if (argument == "--simulate") simulate = true;
      else if (argument == "--store-tr064-password") store_password = true;
      else if (argument == "--migrate-tr064-password") migrate_password = true;
      else if (argument == "--config" && i + 1 < argc) config_path = argv[++i];
      else if (argument == "--version") {
        std::cout << "FritzMonitor " << FRITZMONITOR_VERSION << '\n';
        return 0;
      }
      else if (argument == "--help") {
        std::cout << "Usage: fritzmonitor [--config PATH] [--simulate] [--version]\n"
                     "                    [--store-tr064-password | --migrate-tr064-password]\n";
        return 0;
      }
    }

    if (store_password && migrate_password)
      throw std::runtime_error("choose either --store-tr064-password or --migrate-tr064-password");

    auto config = load_config(config_path);
    SensitiveStringGuard config_password_guard{&config.tr064_password};
    if (store_password) {
      if (config.tr064_password_from_config)
        throw std::runtime_error(
            "configuration still contains tr064_password; use --migrate-tr064-password instead");
      auto password = read_password("TR-064 password: ");
      auto confirmation = read_password("Repeat TR-064 password: ");
      SensitiveStringGuard password_guard{&password};
      SensitiveStringGuard confirmation_guard{&confirmation};
      if (password.empty()) throw std::runtime_error("TR-064 password must not be empty");
      if (password != confirmation) throw std::runtime_error("passwords do not match");
      store_tr064_password(config, password);
      std::cout << "Stored TR-064 password in Secret Service for " << config.host << ".\n";
      return 0;
    }
    if (migrate_password) {
      if (!config.tr064_password_from_config)
        throw std::runtime_error("configuration does not contain a TR-064 password to migrate");
      store_tr064_password(config, config.tr064_password);
      auto stored = lookup_tr064_password(config);
      SensitiveStringGuard stored_password_guard{stored ? &*stored : nullptr};
      if (!stored || *stored != config.tr064_password)
        throw std::runtime_error("stored TR-064 password could not be verified; configuration was not changed");
      remove_tr064_password_from_config(config_path);
      std::cout << "Migrated TR-064 password to Secret Service and removed it from " << config_path << ".\n";
      return 0;
    }

    resolve_tr064_password(config, lookup_tr064_password);
    Desktop desktop;
    Phonebook phonebook(config);
    std::map<std::string, CallEvent> active_calls;
    auto callback = [&](const CallEvent& raw) {
      auto event = raw;
      if (event.type == EventType::Ring) {
        active_calls[event.connection_id] = event;
        desktop.mark_incoming_call();
        if (config.notify_incoming) desktop.notify(event);
      } else if (event.type == EventType::Connected) {
        if (auto found = active_calls.find(event.connection_id); found != active_calls.end()) {
          found->second.type = EventType::Connected;
        }
      } else if (event.type == EventType::Disconnected) {
        if (auto found = active_calls.find(event.connection_id); found != active_calls.end()) {
          const auto answered = found->second.type == EventType::Connected;
          const auto name = phonebook.lookup(found->second.caller);
          desktop.record_call(CallSummary{found->second.caller, name, found->second.timestamp, answered});
          if (found->second.type == EventType::Ring) {
            found->second.type = EventType::Missed;
            if (config.notify_missed) desktop.notify(found->second);
          }
          active_calls.erase(found);
        }
      }
      std::cerr << event_type_name(event.type) << " connection=" << event.connection_id << '\n';
    };

    if (simulate) {
      callback(*parse_callmonitor_line("RING;1;030123456;101"));
      callback(*parse_callmonitor_line("DISCONNECT;1;0"));
      return 0;
    }
    std::thread phonebook_thread([&phonebook] { phonebook.run(); });
    CallmonitorClient client(
        config, callback, system_resolve,
        [&desktop](bool available) {
          desktop.set_connection_available(available);
        });
#ifdef FRITZMONITOR_HAVE_DESKTOP
    std::thread network_thread([&client] { client.run(); });
    desktop.run();
    phonebook.stop();
    phonebook_thread.join();
    network_thread.detach();
    return 0;
#else
    return client.run();
#endif
  } catch (const std::exception& error) {
    std::cerr << "fritzmonitor: " << error.what() << '\n';
    return 1;
  }
}
