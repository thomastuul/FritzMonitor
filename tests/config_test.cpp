#include "config.hpp"

#include <cassert>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

int main() {
  using namespace fritzmonitor;
  const auto path = std::filesystem::temp_directory_path() /
                    ("fritzmonitor-config-test-" + std::to_string(::getpid()) + ".toml");
  {
    std::ofstream output(path);
    output << "host = \"router.local\"\nport = 1012\nnotify_missed = false\n"
              "tr064_password = \"legacy-secret\"\n";
  }
  const auto config = load_config(path);
  assert(config.host == "router.local");
  assert(config.port == 1012);
  assert(!config.notify_missed);
  assert(config.tr064_password == "legacy-secret");
  assert(config.tr064_password_from_config);

  remove_tr064_password_from_config(path);
  const auto migrated = load_config(path);
  assert(migrated.host == "router.local");
  assert(migrated.tr064_password.empty());
  assert(!migrated.tr064_password_from_config);
  std::filesystem::remove(path);

  const auto target = path.string() + ".target";
  const auto link = path.string() + ".link";
  {
    std::ofstream output(target);
    output << "tr064_password = \"legacy-secret\"\n";
  }
  std::filesystem::create_symlink(target, link);
  bool rejected_symlink = false;
  try {
    remove_tr064_password_from_config(link);
  } catch (const std::runtime_error&) {
    rejected_symlink = true;
  }
  assert(rejected_symlink);
  assert(load_config(target).tr064_password == "legacy-secret");
  std::filesystem::remove(link);
  std::filesystem::remove(target);
}
