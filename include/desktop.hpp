#pragma once

#include "call_event.hpp"

#include <string>

namespace fritzmonitor {

struct CallSummary {
  std::string caller;
  std::string name;
  std::chrono::system_clock::time_point timestamp;
  bool answered;
};

class Desktop {
 public:
  Desktop();
  ~Desktop();
  void notify(const CallEvent& event);
  void mark_incoming_call();
  void record_call(const CallSummary& call);
  void run();
};

}  // namespace fritzmonitor
