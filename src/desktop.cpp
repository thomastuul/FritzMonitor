#include "desktop.hpp"

#include <iostream>
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
std::string message_for(const CallEvent& event) {
  if (event.type == EventType::Ring) return "Eingehender Anruf von " + event.caller;
  if (event.type == EventType::Missed) return "Verpasster Anruf von " + event.caller;
  return event_type_name(event.type);
}
}  // namespace

namespace {

#ifdef FRITZMONITOR_HAVE_DESKTOP
void quit_application(GtkWidget*, gpointer) { gtk_main_quit(); }
GtkWidget* history_menu = nullptr;
AppIndicator* indicator = nullptr;

void set_indicator_icon(bool has_unread_call) {
  app_indicator_set_icon_full(indicator,
                              has_unread_call ? "fritzmonitor-phone-red" : "fritzmonitor-phone-green",
                              has_unread_call ? "Ungelesener eingehender Anruf" : "Keine ungelesenen Anrufe");
}

void menu_shown(GtkWidget*, gpointer) { set_indicator_icon(false); }

gboolean update_indicator_icon(gpointer data) {
  set_indicator_icon(GPOINTER_TO_INT(data) != 0);
  return G_SOURCE_REMOVE;
}

gboolean append_history_item(gpointer data) {
  auto* label = static_cast<std::string*>(data);
  if (history_menu) {
    auto* item = gtk_menu_item_new_with_label(label->c_str());
    gtk_menu_shell_insert(GTK_MENU_SHELL(history_menu), item, 1);
    gtk_widget_show(item);
  }
  delete label;
  return G_SOURCE_REMOVE;
}
#endif

}  // namespace

Desktop::Desktop() {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  int argc = 0;
  char** argv = nullptr;
  gtk_init(&argc, &argv);
  notify_init("FritzMonitor");
  indicator = app_indicator_new("fritzmonitor", "fritzmonitor-phone-green", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
  app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
  auto* menu = gtk_menu_new();
  history_menu = menu;
  g_signal_connect(menu, "show", G_CALLBACK(menu_shown), nullptr);
  auto* status = gtk_menu_item_new_with_label("Verbunden mit FRITZ!Box");
  gtk_widget_set_sensitive(status, FALSE);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), status);
  auto* separator = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
  auto* quit = gtk_menu_item_new_with_label("Beenden");
  g_signal_connect(quit, "activate", G_CALLBACK(quit_application), nullptr);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit);
  gtk_widget_show_all(menu);
  app_indicator_set_menu(indicator, GTK_MENU(menu));
  set_indicator_icon(false);
#endif
}

Desktop::~Desktop() {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  notify_uninit();
#endif
}

void Desktop::notify(const CallEvent& event) {
  const auto message = message_for(event);
#ifdef FRITZMONITOR_HAVE_DESKTOP
  NotifyNotification* notification = notify_notification_new("FritzMonitor", message.c_str(), "phone");
  notify_notification_show(notification, nullptr);
  g_object_unref(notification);
#else
  std::cout << "[notification] " << message << '\n';
#endif
}

void Desktop::add_event(const CallEvent& event) {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  if (event.type == EventType::Ring) g_idle_add(update_indicator_icon, GINT_TO_POINTER(1));
  auto* label = new std::string(event_type_name(event.type) + " " + event.caller);
  g_idle_add(append_history_item, label);
#else
  (void)event;
#endif
}

void Desktop::run() {
#ifdef FRITZMONITOR_HAVE_DESKTOP
  gtk_main();
#endif
}

}  // namespace fritzmonitor
