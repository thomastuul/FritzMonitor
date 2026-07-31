#pragma once

#include "config.hpp"

#include <functional>
#include <optional>
#include <string>

namespace fritzmonitor {

using Tr064SecretLookup = std::function<std::optional<std::string>(const Config&)>;

std::optional<std::string> lookup_tr064_password(const Config& config);
void store_tr064_password(const Config& config, const std::string& password);
void resolve_tr064_password(Config& config, const Tr064SecretLookup& lookup);

}  // namespace fritzmonitor
