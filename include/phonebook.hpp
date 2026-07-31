#pragma once

#include "config.hpp"

#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace fritzmonitor {

class Phonebook {
 public:
  explicit Phonebook(const Config& config);

  void load();
  std::string lookup(const std::string& number) const;

 private:
  const Config& config_;
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::string> names_;
};

}  // namespace fritzmonitor
