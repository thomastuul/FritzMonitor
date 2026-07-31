#include "secret_store.hpp"

#include <libsecret/secret.h>

#include <stdexcept>
#include <utility>

namespace fritzmonitor {
namespace {

const SecretSchema* tr064_schema() {
  static const SecretSchema schema = [] {
    SecretSchema value{};
    value.name = "org.fritzmonitor.Tr064Credentials";
    value.flags = SECRET_SCHEMA_NONE;
    value.attributes[0] = {"application", SECRET_SCHEMA_ATTRIBUTE_STRING};
    value.attributes[1] = {"host", SECRET_SCHEMA_ATTRIBUTE_STRING};
    value.attributes[2] = {"username", SECRET_SCHEMA_ATTRIBUTE_STRING};
    value.attributes[3] = {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING};
    return value;
  }();
  return &schema;
}

std::runtime_error secret_service_error(const char* action, GError* error) {
  std::string message = std::string(action) + ": ";
  message += error ? error->message : "unknown Secret Service error";
  if (error) g_error_free(error);
  return std::runtime_error(message);
}

}  // namespace

std::optional<std::string> lookup_tr064_password(const Config& config) {
  GError* error = nullptr;
  gchar* password = secret_password_lookup_sync(
      tr064_schema(), nullptr, &error, "application", "fritzmonitor", "host", config.host.c_str(),
      "username", config.tr064_username.c_str(), nullptr);
  if (error) throw secret_service_error("cannot read TR-064 password from Secret Service", error);
  if (!password) return std::nullopt;
  std::string result(password);
  secret_password_free(password);
  return result;
}

void store_tr064_password(const Config& config, const std::string& password) {
  const auto label = "FritzMonitor TR-064 (" + config.host + ")";
  GError* error = nullptr;
  const auto stored = secret_password_store_sync(
      tr064_schema(), SECRET_COLLECTION_DEFAULT, label.c_str(), password.c_str(), nullptr, &error,
      "application", "fritzmonitor", "host", config.host.c_str(), "username",
      config.tr064_username.c_str(), nullptr);
  if (!stored) throw secret_service_error("cannot store TR-064 password in Secret Service", error);
}

void resolve_tr064_password(Config& config, const Tr064SecretLookup& lookup) {
  if (!config.addressbook_enabled || !config.tr064_password.empty()) return;
  auto password = lookup(config);
  if (!password || password->empty()) {
    throw std::runtime_error(
        "TR-064 password is missing; run fritzmonitor --store-tr064-password or set "
        "FRITZMONITOR_TR064_PASSWORD");
  }
  config.tr064_password = std::move(*password);
}

}  // namespace fritzmonitor
