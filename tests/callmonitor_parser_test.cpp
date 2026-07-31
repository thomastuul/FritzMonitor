#include "call_event.hpp"

#include <cassert>

int main() {
  using namespace fritzmonitor;
  const auto ring = parse_callmonitor_line("RING;7;030123456;101\r\n");
  assert(ring.has_value());
  assert(ring->type == EventType::Ring);
  assert(ring->connection_id == "7");
  assert(ring->caller == "030123456");
  assert(ring->callee == "101");
  const auto timestamped_ring = parse_callmonitor_line("31.07.26 07:33:10;RING;2;08931883740;31883741;SIP1;\r\n");
  assert(timestamped_ring.has_value());
  assert(timestamped_ring->type == EventType::Ring);
  assert(timestamped_ring->connection_id == "2");
  const auto timestamped_call = parse_callmonitor_line("31.07.26 07:33:10;CALL;1;11;31883740;08931883741;SIP0;\r\n");
  assert(timestamped_call.has_value());
  assert(timestamped_call->type == EventType::CallStarted);
  assert(timestamped_call->connection_id == "1");
  assert(parse_callmonitor_line("UNKNOWN;1").has_value() == false);
  const auto disconnect = parse_callmonitor_line("DISCONNECT;7;12");
  assert(disconnect.has_value());
  assert(disconnect->type == EventType::Disconnected);
}
