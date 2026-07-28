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
  assert(parse_callmonitor_line("UNKNOWN;1").has_value() == false);
  const auto disconnect = parse_callmonitor_line("DISCONNECT;7;12");
  assert(disconnect.has_value());
  assert(disconnect->type == EventType::Disconnected);
}
