#pragma once

#include "call_event.hpp"
#include "config.hpp"

#include <functional>

namespace fritzmonitor {

using EventCallback = std::function<void(const CallEvent&)>;

class CallmonitorClient {
 public:
  CallmonitorClient(Config config, EventCallback callback);
  int run();

 private:
  Config config_;
  EventCallback callback_;
};

}  // namespace fritzmonitor
