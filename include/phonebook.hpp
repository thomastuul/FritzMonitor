#pragma once

#include "config.hpp"
#include "network_target.hpp"

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace fritzmonitor {

struct PhonebookRequestPlan {
  ResolvedHttpUrl target;
  bool send_tr064_credentials = false;
};

PhonebookRequestPlan prepare_tr064_request(
    const Config& config,
    const AddressResolver& resolver = system_resolve);
PhonebookRequestPlan prepare_phonebook_download(
    const std::string& url, const Config& config,
    const AddressResolver& resolver = system_resolve);
bool perform_phonebook_request(const PhonebookRequestPlan& plan,
                               const Config& config, const std::string* post,
                               const std::string& soap_action,
                               std::string& response);

class Phonebook {
 public:
  explicit Phonebook(const Config& config,
                     AddressResolver resolver = system_resolve);

  void load();
  void run();
  void stop();
  std::string lookup(const std::string& number) const;

 private:
  bool load_once();
  const Config& config_;
  AddressResolver resolver_;
  std::mutex stop_mutex_;
  std::condition_variable stop_condition_;
  bool stopped_ = false;
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::string> names_;
};

}  // namespace fritzmonitor
