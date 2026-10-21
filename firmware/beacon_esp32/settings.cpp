#include <Preferences.h>
#include "beacon.h"

Settings settings;
static Preferences prefs;

void settings_load() {
  prefs.begin("ctv1", true);
  settings.wifi_ssid = prefs.getString("ssid", "");
  settings.wifi_pass = prefs.getString("wpass", "");
  settings.mqtt_host = prefs.getString("mhost", "");
  settings.mqtt_port = prefs.getUShort("mport", 1883);
  settings.mqtt_user = prefs.getString("muser", "");
  settings.mqtt_pass = prefs.getString("mpass", "");
  settings.mqtt_tls = prefs.getBool("mtls", false);
  settings.mqtt_ca = prefs.getString("mca", "");
  settings.gw_id = prefs.getString("gw", "home");
  settings.admin_pass = prefs.getString("admin", "");
  settings.siren = prefs.getBool("siren", true);
  prefs.end();
}

void settings_save() {
  prefs.begin("ctv1", false);
  prefs.putString("ssid", settings.wifi_ssid);
  prefs.putString("wpass", settings.wifi_pass);
  prefs.putString("mhost", settings.mqtt_host);
  prefs.putUShort("mport", settings.mqtt_port);
  prefs.putString("muser", settings.mqtt_user);
  prefs.putString("mpass", settings.mqtt_pass);
  prefs.putBool("mtls", settings.mqtt_tls);
  prefs.putString("mca", settings.mqtt_ca);
  prefs.putString("gw", settings.gw_id);
  prefs.putString("admin", settings.admin_pass);
  prefs.putBool("siren", settings.siren);
  prefs.end();
}

String settings_load_fence() {
  prefs.begin("ctv1", true);
  String s = prefs.getString("fence", "");
  prefs.end();
  return s;
}

void settings_save_fence(const String &json) {
  prefs.begin("ctv1", false);
  prefs.putString("fence", json);
  prefs.end();
}
