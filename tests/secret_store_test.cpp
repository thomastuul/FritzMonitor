#include "secret_store.hpp"

#include <cassert>
#include <optional>
#include <stdexcept>

int main() {
  using namespace fritzmonitor;

  Config disabled;
  bool called = false;
  resolve_tr064_password(disabled, [&](const Config&) {
    called = true;
    return std::optional<std::string>{"unused"};
  });
  assert(!called);

  Config configured;
  configured.addressbook_enabled = true;
  configured.tr064_password = "legacy";
  resolve_tr064_password(configured, [&](const Config&) {
    called = true;
    return std::optional<std::string>{"unused"};
  });
  assert(configured.tr064_password == "legacy");
  assert(!called);

  Config keyring;
  keyring.addressbook_enabled = true;
  resolve_tr064_password(keyring, [](const Config& config) {
    assert(config.host == "fritz.box");
    return std::optional<std::string>{"from-keyring"};
  });
  assert(keyring.tr064_password == "from-keyring");

  Config missing;
  missing.addressbook_enabled = true;
  bool failed = false;
  try {
    resolve_tr064_password(missing, [](const Config&) { return std::optional<std::string>{}; });
  } catch (const std::runtime_error&) {
    failed = true;
  }
  assert(failed);
}
