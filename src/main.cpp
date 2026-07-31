#include "call_event.hpp"
#include "callmonitor_client.hpp"
#include "config.hpp"
#include "desktop.hpp"

#include <iostream>
#include <map>
#include <string>
#include <thread>

int main(int argc, char** argv) {
  using namespace fritzmonitor;
  try {
    auto config_path = default_config_path();
    bool simulate = false;
    for (int i = 1; i < argc; ++i) {
      const std::string argument = argv[i];
      if (argument == "--simulate") simulate = true;
      else if (argument == "--config" && i + 1 < argc) config_path = argv[++i];
      else if (argument == "--help") {
        std::cout << "Usage: fritzmonitor [--config PATH] [--simulate]\n";
        return 0;
      }
    }

    const auto config = load_config(config_path);
    Desktop desktop;
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
          desktop.record_call(CallSummary{found->second.caller, "", found->second.timestamp, answered});
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
    CallmonitorClient client(config, callback);
#ifdef FRITZMONITOR_HAVE_DESKTOP
    std::thread network_thread([&client] { client.run(); });
    desktop.run();
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
