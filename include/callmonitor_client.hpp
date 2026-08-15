#pragma once

#include "call_event.hpp"
#include "config.hpp"
#include "network_target.hpp"

#include <functional>

namespace fritzmonitor {

using EventCallback = std::function<void(const CallEvent&)>;

class CallmonitorClient {
 public:
  CallmonitorClient(Config config, EventCallback callback,
                    AddressResolver resolver = system_resolve);
  int run();

 private:
  Config config_;
  EventCallback callback_;
  AddressResolver resolver_;
};

}  // namespace fritzmonitor
