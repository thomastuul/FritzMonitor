#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace fritzmonitor {

enum class EventType { Ring, CallStarted, Connected, Disconnected, Incoming, Missed };

struct CallEvent {
  EventType type;
  std::string connection_id;
  std::string caller;
  std::string callee;
  std::chrono::system_clock::time_point timestamp;
};

std::string event_type_name(EventType type);
std::optional<CallEvent> parse_callmonitor_line(const std::string& line);

}  // namespace fritzmonitor
