#include "phonebook.hpp"

#ifdef FRITZMONITOR_HAVE_PHONEBOOK
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fritzmonitor {
namespace {

std::string normalize_number(const std::string& number) {
  std::string digits;
  for (const unsigned char character : number) {
    if (std::isdigit(character)) digits.push_back(static_cast<char>(character));
  }
  if (digits.rfind("0049", 0) == 0) digits.replace(0, 4, "0");
  else if (digits.rfind("49", 0) == 0) digits.replace(0, 2, "0");
  return digits;
}

#ifdef FRITZMONITOR_HAVE_PHONEBOOK
size_t write_response(char* data, size_t size, size_t count, void* user_data) {
  auto* response = static_cast<std::string*>(user_data);
  response->append(data, size * count);
  return size * count;
}

std::string element_name(const xmlNode* node) {
  return node && node->name ? reinterpret_cast<const char*>(node->name) : std::string{};
}

std::string node_text(const xmlNode* node) {
  if (!node) return {};
  xmlChar* content = xmlNodeGetContent(const_cast<xmlNode*>(node));
  if (!content) return {};
  std::string text(reinterpret_cast<const char*>(content));
  xmlFree(content);
  return text;
}

const xmlNode* find_element(const xmlNode* node, const std::string& wanted) {
  for (auto* current = node; current; current = current->next) {
    if (current->type == XML_ELEMENT_NODE && element_name(current) == wanted) return current;
    if (const auto* found = find_element(current->children, wanted)) return found;
  }
  return nullptr;
}

void collect_contact(const xmlNode* contact, std::unordered_map<std::string, std::string>& names) {
  std::string name;
  for (auto* child = contact->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE) continue;
    if (element_name(child) == "person") {
      if (const auto* real_name = find_element(child->children, "realName")) name = node_text(real_name);
    }
  }
  if (name.empty()) return;

  for (auto* child = contact->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE || element_name(child) != "telephony") continue;
    for (auto* number = child->children; number; number = number->next) {
      if (number->type != XML_ELEMENT_NODE || element_name(number) != "number") continue;
      const auto normalized = normalize_number(node_text(number));
      if (!normalized.empty()) names[normalized] = name;
    }
  }
}

void collect_contacts(const xmlNode* node, std::unordered_map<std::string, std::string>& names) {
  for (auto* current = node; current; current = current->next) {
    if (current->type == XML_ELEMENT_NODE && element_name(current) == "contact") {
      collect_contact(current, names);
    } else {
      collect_contacts(current->children, names);
    }
  }
}

bool fetch(const PhonebookRequestPlan& plan, const Config& config,
           const std::string* post, const std::string& soap_action,
           std::string& response) {
  CURL* curl = curl_easy_init();
  if (!curl) return false;
  const auto resolve_entry = curl_resolve_entry(plan.target.target);
  struct curl_slist* resolve = nullptr;
  resolve = curl_slist_append(resolve, resolve_entry.c_str());
  if (!resolve) {
    curl_easy_cleanup(curl);
    return false;
  }
  if (curl_easy_setopt(curl, CURLOPT_URL, plan.target.url.c_str()) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_NOPROXY, "*") != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_IGNORED) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 0L) != CURLE_OK) {
    curl_slist_free_all(resolve);
    curl_easy_cleanup(curl);
    return false;
  }
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 8000L);
#if LIBCURL_VERSION_NUM >= 0x075500
  if (curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https") != CURLE_OK) {
#else
  if (curl_easy_setopt(curl, CURLOPT_PROTOCOLS,
                       CURLPROTO_HTTP | CURLPROTO_HTTPS) != CURLE_OK) {
#endif
    curl_slist_free_all(resolve);
    curl_easy_cleanup(curl);
    return false;
  }
#if LIBCURL_VERSION_NUM >= 0x073d00
  if (curl_easy_setopt(curl, CURLOPT_DISALLOW_USERNAME_IN_URL, 1L) != CURLE_OK) {
    curl_slist_free_all(resolve);
    curl_easy_cleanup(curl);
    return false;
  }
#endif
  if (plan.send_tr064_credentials) {
    if (curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_USERNAME,
                         config.tr064_username.c_str()) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_PASSWORD,
                         config.tr064_password.c_str()) != CURLE_OK) {
      curl_slist_free_all(resolve);
      curl_easy_cleanup(curl);
      return false;
    }
  }

  struct curl_slist* headers = nullptr;
  if (post) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post->c_str());
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=\"utf-8\"");
    const auto action_header = "SOAPAction: \"urn:dslforum-org:service:X_AVM-DE_OnTel:1#" + soap_action + "\"";
    headers = curl_slist_append(headers, action_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  const auto result = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  if (headers) curl_slist_free_all(headers);
  curl_slist_free_all(resolve);
  curl_easy_cleanup(curl);
  return result == CURLE_OK && status >= 200 && status < 300;
}

std::string soap_request(const std::string& action, const std::string& body) {
  return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
         "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
         "<s:Body><u:" + action + " xmlns:u=\"urn:dslforum-org:service:X_AVM-DE_OnTel:1\">" + body +
         "</u:" + action + "></s:Body></s:Envelope>";
}

std::string property(const xmlNode* node, const char* name) {
  if (!node) return {};
  xmlChar* value = xmlGetProp(const_cast<xmlNode*>(node), reinterpret_cast<const xmlChar*>(name));
  if (!value) return {};
  std::string result(reinterpret_cast<const char*>(value));
  xmlFree(value);
  return result;
}

void collect_phonebook_ids(const xmlNode* node, std::vector<std::string>& ids) {
  for (auto* current = node; current; current = current->next) {
    if (current->type == XML_ELEMENT_NODE &&
        (element_name(current) == "phonebook" || element_name(current) == "Phonebook")) {
      auto id = property(current, "id");
      if (id.empty()) id = property(current, "ID");
      if (id.empty()) {
        for (auto* child = current->children; child; child = child->next) {
          if (child->type != XML_ELEMENT_NODE) continue;
          if (element_name(child) == "id" || element_name(child) == "ID" ||
              element_name(child) == "PhonebookID") {
            id = node_text(child);
            break;
          }
        }
      }
      if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
    collect_phonebook_ids(current->children, ids);
  }
}

void parse_phonebook_list(const std::string& value, std::vector<std::string>& ids) {
  std::string id;
  for (const unsigned char character : value) {
    if (std::isdigit(character)) {
      id.push_back(static_cast<char>(character));
    } else if (!id.empty()) {
      if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
      id.clear();
    }
  }
  if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
}
#endif

}  // namespace

PhonebookRequestPlan prepare_tr064_request(const Config& config,
                                           const AddressResolver& resolver) {
  const auto endpoint = http_url(
      config.host, static_cast<std::uint16_t>(config.tr064_port),
      "/upnp/control/x_contact");
  return PhonebookRequestPlan{
      resolve_trusted_http_url(endpoint, config.allow_nonlocal_addresses,
                               resolver),
      true};
}

PhonebookRequestPlan prepare_phonebook_download(
    const std::string& url, const Config& config,
    const AddressResolver& resolver) {
  return PhonebookRequestPlan{
      resolve_trusted_http_url(url, config.allow_nonlocal_addresses, resolver),
      false};
}

bool perform_phonebook_request(const PhonebookRequestPlan& plan,
                               const Config& config, const std::string* post,
                               const std::string& soap_action,
                               std::string& response) {
#ifdef FRITZMONITOR_HAVE_PHONEBOOK
  return fetch(plan, config, post, soap_action, response);
#else
  (void)plan;
  (void)config;
  (void)post;
  (void)soap_action;
  (void)response;
  return false;
#endif
}

Phonebook::Phonebook(const Config& config, AddressResolver resolver)
    : config_(config), resolver_(std::move(resolver)) {}

void Phonebook::load() {
#ifndef FRITZMONITOR_HAVE_PHONEBOOK
  if (config_.addressbook_enabled) {
    std::cerr << "fritzmonitor: address book support is not available in this build\n";
  }
#else
  if (!config_.addressbook_enabled) return;
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return;
  load_once();
  curl_global_cleanup();
#endif
}

bool Phonebook::load_once() {
#ifndef FRITZMONITOR_HAVE_PHONEBOOK
  return false;
#else
  PhonebookRequestPlan endpoint;
  try {
    endpoint = prepare_tr064_request(config_, resolver_);
  } catch (const TargetResolutionError&) {
    return false;
  }
  std::unordered_map<std::string, std::string> loaded_names;
  std::vector<std::string> phonebook_ids;
  const auto list_request = soap_request("GetPhonebookList", "");
  std::string list_response;
  if (perform_phonebook_request(endpoint, config_, &list_request,
                                "GetPhonebookList", list_response)) {
    auto* list = xmlReadMemory(list_response.data(), static_cast<int>(list_response.size()), nullptr, nullptr, XML_PARSE_NONET);
    if (list) {
      const auto* list_node = find_element(xmlDocGetRootElement(list), "NewPhonebookList");
      parse_phonebook_list(node_text(list_node), phonebook_ids);
      collect_phonebook_ids(xmlDocGetRootElement(list), phonebook_ids);
      xmlFreeDoc(list);
    }
  }
  if (phonebook_ids.empty()) {
    phonebook_ids.push_back("0");
  }

  std::size_t loaded_phonebooks = 0;
  for (const auto& id : phonebook_ids) {
    const auto request = soap_request("GetPhonebook", "<NewPhonebookID>" + id + "</NewPhonebookID>");
    std::string soap_response;
    if (!perform_phonebook_request(endpoint, config_, &request,
                                   "GetPhonebook", soap_response))
      continue;
    auto* soap = xmlReadMemory(soap_response.data(), static_cast<int>(soap_response.size()), nullptr, nullptr, XML_PARSE_NONET);
    const auto* url_node = soap ? find_element(xmlDocGetRootElement(soap), "NewPhonebookURL") : nullptr;
    const auto phonebook_url = node_text(url_node);
    if (soap) xmlFreeDoc(soap);
    if (phonebook_url.empty()) continue;

    PhonebookRequestPlan download;
    try {
      download = prepare_phonebook_download(phonebook_url, config_, resolver_);
    } catch (const TargetResolutionError&) {
      continue;
    }
    std::string phonebook_response;
    if (!perform_phonebook_request(download, config_, nullptr, "",
                                   phonebook_response))
      continue;
    auto* phonebook = xmlReadMemory(phonebook_response.data(), static_cast<int>(phonebook_response.size()), nullptr, nullptr, XML_PARSE_NONET);
    if (!phonebook) continue;
    collect_contacts(xmlDocGetRootElement(phonebook), loaded_names);
    xmlFreeDoc(phonebook);
    ++loaded_phonebooks;
  }
  if (loaded_phonebooks == 0) return false;
  {
    std::unique_lock lock(mutex_);
    names_ = std::move(loaded_names);
  }
  std::shared_lock lock(mutex_);
  std::cerr << "fritzmonitor: loaded " << loaded_phonebooks << " phonebooks and " << names_.size()
            << " phonebook numbers\n";
  return true;
#endif
}

void Phonebook::run() {
#ifndef FRITZMONITOR_HAVE_PHONEBOOK
  load();
#else
  if (!config_.addressbook_enabled) return;
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    std::cerr << "fritzmonitor: cannot initialize address book support\n";
    return;
  }
  RetryBackoff backoff(config_.reconnect_seconds,
                       config_.reconnect_max_seconds);
  FailureLogState failure_log;
  while (true) {
    {
      std::lock_guard lock(stop_mutex_);
      if (stopped_) break;
    }
    if (load_once()) break;
    const int delay = backoff.next_delay_seconds();
    if (failure_log.begin_failure())
      std::cerr << "fritzmonitor: address book unavailable; retrying with "
                   "network backoff\n";
    std::unique_lock lock(stop_mutex_);
    if (stop_condition_.wait_for(lock, std::chrono::seconds(delay),
                                 [this] { return stopped_; }))
      break;
  }
  curl_global_cleanup();
#endif
}

void Phonebook::stop() {
  {
    std::lock_guard lock(stop_mutex_);
    stopped_ = true;
  }
  stop_condition_.notify_all();
}

std::string Phonebook::lookup(const std::string& number) const {
  const auto normalized = normalize_number(number);
  std::shared_lock lock(mutex_);
  const auto found = names_.find(normalized);
  if (found != names_.end()) return found->second;
  if (normalized.size() < 7) return {};

  const auto suffix = normalized.substr(normalized.size() - 7);
  std::string match;
  for (const auto& [stored_number, name] : names_) {
    if (stored_number.size() < 7 || stored_number.substr(stored_number.size() - 7) != suffix) continue;
    if (!match.empty() && match != name) return {};
    match = name;
  }
  return match;
}

}  // namespace fritzmonitor
