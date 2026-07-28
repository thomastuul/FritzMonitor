#pragma once

#include "call_event.hpp"

namespace fritzmonitor {

class Desktop {
 public:
  Desktop();
  ~Desktop();
  void notify(const CallEvent& event);
  void add_event(const CallEvent& event);
  void run();
};

}  // namespace fritzmonitor
