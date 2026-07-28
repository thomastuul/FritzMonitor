#include "config.hpp"

#include <cassert>
#include <fstream>

int main() {
  using namespace fritzmonitor;
  const auto path = std::filesystem::temp_directory_path() / "fritzmonitor-config-test.toml";
  {
    std::ofstream output(path);
    output << "host = \"router.local\"\nport = 1012\nnotify_missed = false\n";
  }
  const auto config = load_config(path);
  assert(config.host == "router.local");
  assert(config.port == 1012);
  assert(!config.notify_missed);
  std::filesystem::remove(path);
}
