#include "call_event.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace fritzmonitor {
namespace {

std::vector<std::string> split(const std::string& input) {
  std::vector<std::string> result;
  std::stringstream stream(input);
  std::string part;
  while (std::getline(stream, part, ';')) result.push_back(part);
  return result;
}

}  // namespace

std::string event_type_name(EventType type) {
  switch (type) {
    case EventType::Ring: return "ring";
    case EventType::CallStarted: return "call_started";
    case EventType::Connected: return "connected";
    case EventType::Disconnected: return "disconnected";
    case EventType::Incoming: return "incoming";
    case EventType::Missed: return "missed";
  }
  return "unknown";
}

std::optional<CallEvent> parse_callmonitor_line(const std::string& line) {
  auto fields = split(line);
  if (fields.size() < 2) return std::nullopt;
  while (!fields.back().empty() && (fields.back().back() == '\r' || fields.back().back() == '\n'))
    fields.back().pop_back();

  const auto now = std::chrono::system_clock::now();
  if (fields[0] == "RING" && fields.size() >= 4)
    return CallEvent{EventType::Ring, fields[1], fields[2], fields[3], now};
  if (fields[0] == "CALL" && fields.size() >= 5)
    return CallEvent{EventType::CallStarted, fields[1], fields[3], fields[4], now};
  if (fields[0] == "CONNECT" && fields.size() >= 3)
    return CallEvent{EventType::Connected, fields[1], "", fields[2], now};
  if (fields[0] == "DISCONNECT" && fields.size() >= 2)
    return CallEvent{EventType::Disconnected, fields[1], "", "", now};
  return std::nullopt;
}

}  // namespace fritzmonitor
