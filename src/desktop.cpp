#include "desktop.hpp"

#include <cstdlib>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifdef FRITZMONITOR_HAVE_DESKTOP
#include <gtk/gtk.h>
#include <libnotify/notify.h>

extern "C" {
#include <libayatana-appindicator/app-indicator.h>
}
#endif

namespace fritzmonitor {

namespace {
bool german_locale() {
  for (const char* variable : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
    if (const char* value = std::getenv(variable); value && std::string(value).rfind("de", 0) == 0) return true;
  }
  return false;
}

std::string format_call(const CallSummary& call) {
  const auto local_time = std::chrono::system_clock::to_time_t(call.timestamp);
  std::tm time{};
  localtime_r(&local_time, &time);
  std::ostringstream output;
  output << std::put_time(&time, "%d.%m. %H:%M") << " - ";
  output << (call.name.empty() ? call.caller : call.name);
  return output.str();
}

std::string message_for(const CallEvent& event) {
  if (event.type == EventType::Ring) {
    return german_locale() ? "Eingehender Anruf von " + event.caller
                           : "Incoming call from " + event.caller;
  }
  if (event.type == EventType::Missed) {
    return german_locale() ? "Verpasster Anruf von " + event.caller
                           : "Missed call from " + event.caller;
  }
  return event_type_name(event.type);
}
}  // namespace

namespace {

#ifdef FRITZMONITOR_HAVE_DESKTOP
void quit_application(GtkWidget*, gpointer) { gtk_main_quit(); }
GtkWidget* history_menu = nullptr;
GtkWidget* connection_status_item = nullptr;
AppIndicator* indicator = nullptr;
std::deque<GtkWidget*> call_items;
bool notifications_initialized = false;
bool notifications_available = false;
bool connection_available = false;
bool has_unread_call = false;

void refresh_indicator() {
  const char* icon;
  const char* description;
  const char* status_label;
  if (!connection_available) {
    icon = "fritzmonitor-phone-yellow";
    description = german_locale() ? "Keine Verbindung zur FRITZ!Box"
                                  : "No connection to FRITZ!Box";
    status_label = german_locale() ? "Nicht mit FRITZ!Box verbunden"
                                   : "Not connected to FRITZ!Box";
  } else if (has_unread_call) {
    icon = "fritzmonitor-phone-red";
    description = german_locale() ? "Ungelesener eingehender Anruf"
                                  : "Unread incoming call";
    status_label = german_locale() ? "Verbunden mit FRITZ!Box"
                                   : "Connected to FRITZ!Box";
  } else {
    icon = "fritzmonitor-phone-green";
    description = german_locale() ? "Keine ungelesenen Anrufe"
                                  : "No unread calls";
    status_label = german_locale() ? "Verbunden mit FRITZ!Box"
                                   : "Connected to FRITZ!Box";
  }
  app_indicator_set_icon_full(indicator, icon, description);
  if (connection_status_item)
    gtk_menu_item_set_label(GTK_MENU_ITEM(connection_status_item), status_label);
}

void menu_shown(GtkWidget*, gpointer) {
  has_unread_call = false;
  refresh_indicator();
}

enum class IndicatorUpdate { MarkUnread = 1, Connected, Disconnected };

gboolean update_indicator_icon(gpointer data) {
  switch (static_cast<IndicatorUpdate>(GPOINTER_TO_INT(data))) {
    case IndicatorUpdate::MarkUnread: has_unread_call = true; break;
    case IndicatorUpdate::Connected: connection_available = true; break;
    case IndicatorUpdate::Disconnected: connection_available = false; break;
  }
  refresh_indicator();
  return G_SOURCE_REMOVE;
}

gboolean append_history_item(gpointer data) {
  auto* call = static_cast<CallSummary*>(data);
  if (history_menu) {
    const auto label = format_call(*call);
    auto* item = gtk_menu_item_new();
    auto* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    auto* icon = gtk_image_new_from_icon_name(
        call->answered ? "fritzmonitor-phone-green" : "fritzmonitor-phone-red", GTK_ICON_SIZE_MENU);
    auto* text = gtk_label_new(label.c_str());
    gtk_label_set_xalign(GTK_LABEL(text), 0.0F);
    gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), text, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(item), row);
    gtk_widget_set_tooltip_text(
        item, german_locale() ? (call->answered ? "Angenommener Anruf" : "Verpasster Anruf")
                               : (call->answered ? "Answered call" : "Missed call"));
    gtk_menu_shell_insert(GTK_MENU_SHELL(history_menu), item, 2);
    gtk_widget_show_all(item);
    call_items.push_front(item);
    if (call_items.size() > 3) {
      gtk_widget_destroy(call_items.back());
      call_items.pop_back();
    }
  }
  delete call;
  return G_SOURCE_REMOVE;
}
#endif

}  // namespace

Desktop::Desktop() {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  int argc = 0;
  char** argv = nullptr;
  gtk_init(&argc, &argv);
  notifications_initialized = notify_init("FritzMonitor");
  if (notifications_initialized) {
    gchar* server_name = nullptr;
    gchar* server_vendor = nullptr;
    gchar* server_version = nullptr;
    gchar* server_spec = nullptr;
    notifications_available = notify_get_server_info(&server_name, &server_vendor, &server_version, &server_spec);
    g_free(server_name);
    g_free(server_vendor);
    g_free(server_version);
    g_free(server_spec);
  }
  if (!notifications_available) {
    std::cerr << "fritzmonitor: desktop notification service unavailable; continuing without notifications\n";
  }
  indicator = app_indicator_new("fritzmonitor", "fritzmonitor-phone-yellow", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
  app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
  auto* menu = gtk_menu_new();
  history_menu = menu;
  g_signal_connect(menu, "show", G_CALLBACK(menu_shown), nullptr);
  const auto status_label = german_locale() ? "Nicht mit FRITZ!Box verbunden" : "Not connected to FRITZ!Box";
  auto* status = gtk_menu_item_new_with_label(status_label);
  connection_status_item = status;
  gtk_widget_set_sensitive(status, FALSE);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), status);
  auto* separator = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  auto* quit = gtk_menu_item_new_with_label(german_locale() ? "Beenden" : "Quit");
  g_signal_connect(quit, "activate", G_CALLBACK(quit_application), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);
  gtk_widget_show_all(menu);
  app_indicator_set_menu(indicator, GTK_MENU(menu));
  refresh_indicator();
#endif
}

Desktop::~Desktop() {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  if (notifications_initialized) notify_uninit();
#endif
}

void Desktop::notify(const CallEvent& event) {
  const auto message = message_for(event);
#ifdef FRITZMONITOR_HAVE_DESKTOP
  if (!notifications_available) return;
  NotifyNotification* notification = notify_notification_new("FritzMonitor", message.c_str(), "fritzmonitor-phone-green");
  notify_notification_set_category(notification, "phone.call");
  notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
  notify_notification_set_timeout(notification, 5000);
  notify_notification_show(notification, nullptr);
  g_object_unref(notification);
#else
  std::cout << "[notification] " << message << '\n';
#endif
}

void Desktop::mark_incoming_call() {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  g_idle_add(update_indicator_icon,
             GINT_TO_POINTER(static_cast<int>(IndicatorUpdate::MarkUnread)));
#endif
}

void Desktop::set_connection_available(bool available) {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  const auto update = available ? IndicatorUpdate::Connected
                                : IndicatorUpdate::Disconnected;
  g_idle_add(update_indicator_icon,
             GINT_TO_POINTER(static_cast<int>(update)));
#else
  (void)available;
#endif
}

void Desktop::record_call(const CallSummary& call) {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  g_idle_add(append_history_item, new CallSummary(call));
#else
  (void)call;
#endif
}

void Desktop::run() {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  gtk_main();
#endif
}

}  // namespace fritzmonitor
