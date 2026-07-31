# FritzMonitor

## Version und Copyright

FritzMonitor verwendet das Schema `MAJOR.MINOR.PATCH` (SemVer). Der aktuelle
Stand ist `0.2.0`; Debian- und RPM-Pakete verwenden zusätzlich ein Paket-
Release, derzeit `1`. Das Projekt wurde erstellt und gestaltet von Thomas
Tuul zusammen mit OpenAI Codex. Die vollständige Attribution steht in
`COPYRIGHT.md`.

FritzMonitor soll als native Linux-Systemtray-Anwendung Ereignisse einer FRITZ!Box empfangen und den Benutzer per Desktop-Benachrichtigung informieren.

## MVP

Der erste funktionsfähige Umfang konzentriert sich auf Telefonie-Ereignisse über den FRITZ!Box-Callmonitor auf TCP-Port 1012:

- eingehende Anrufe sofort melden
- angenommene und beendete Anrufe verfolgen
- nicht angenommene Anrufe als verpasste Anrufe melden
- Verbindungsstatus und die letzten Ereignisse im Tray anzeigen
- bei einer Netzwerkunterbrechung automatisch wieder verbinden

Weitere FRITZ!Box-Ereignisse über TR-064/UPnP können später als zusätzliche Adapter ergänzt werden.

## Desktop-Benachrichtigungen

FritzMonitor verwendet die `libnotify`-Bibliothek direkt. Die Benachrichtigung
wird über D-Bus an den Desktop-Benachrichtigungsdienst gesendet; ein installiertes
`notify-send` ist nicht erforderlich. `notify-send` und FritzMonitor verwenden
damit dieselbe standardisierte Benachrichtigungsschnittstelle.

Die Meldungen verwenden den Anwendungsnamen `FritzMonitor`, das installierte
Telefon-Icon, die Kategorie `phone.call`, normale Dringlichkeit und eine
Anzeigedauer von fünf Sekunden. Wenn keine D-Bus-Benachrichtigungsinstanz
verfügbar ist, läuft FritzMonitor ohne Desktop-Meldungen weiter und protokolliert
dies im User-Journal. Ein Shell-Aufruf von `notify-send` wird nicht als Fallback
verwendet.

## Tray-Icon und Anrufstatus

Das Systemtray verwendet ein telefonförmiges FritzMonitor-Icon mit zwei
Zuständen:

- Grün: Seit dem Start des Monitors ist kein eingehender Anruf eingegangen
  oder alle eingegangenen Anrufe wurden durch das Öffnen des Pulldown-Menüs
  abgefragt.
- Rot: Seit dem Start ist mindestens ein eingehender Anruf eingegangen, der
  noch nicht durch das Öffnen des Pulldown-Menüs abgefragt wurde.

Das Öffnen des Pulldown-Menüs bestätigt den aktuellen Anrufstatus und setzt
das Icon wieder auf Grün. Die grünen und roten SVG-Icons werden zusammen mit
der nativen Desktop-Binärdatei installiert.

Das Pulldown-Menü zeigt pro Anruf genau eine Zeile in dieser Reihenfolge:

1. grüner Telefonhörer bei einem angenommenen Anruf oder roter Telefonhörer
   bei einem verpassten Anruf
2. Datum und Uhrzeit im Format `12.07. 18:23`
3. Name oder, falls kein Name zugeordnet werden kann, die Rufnummer

Eine Namensauflösung kann optional über das FRITZ!Box-Adressbuch erfolgen; wenn
keine Auflösung verfügbar ist, darf der Anruf trotzdem nicht verworfen werden.
Es werden immer nur die letzten drei Anrufe angezeigt, der neueste Eintrag
steht oben. Bei deutscher Systemlokalisierung sind Menütexte und Tooltips
deutsch, ansonsten englisch.

## Namensauflösung über das FRITZ!Box-Adressbuch

Die Namensauflösung über das FRITZ!Box-Adressbuch ist eine optionale
Erweiterung. FritzMonitor verwendet dafür die lokale TR-064/UPnP-Schnittstelle
der FRITZ!Box, insbesondere den Telefonbuchdienst `X_AVM-DE_OnTel`.

- Alle über TR-064 verfügbaren Telefonbücher werden beim Programmstart geladen
  und gemeinsam im Speicher gehalten.
- Rufnummern werden vor dem Vergleich normalisiert, damit unterschiedliche
  Schreibweisen wie `+49...` und `0...` zugeordnet werden können.
- Für einen Treffer wird der Name zusätzlich zur Rufnummer im Pulldown-Menü
  angezeigt.
- Ist die FRITZ!Box nicht erreichbar, TR-064 deaktiviert oder die Abfrage nicht
  berechtigt, bleibt FritzMonitor funktionsfähig und zeigt die Rufnummer an.
- Zugangsdaten werden ausschließlich über die lokale Konfiguration oder eine
  dafür vorgesehene Umgebungsvariable bezogen und nicht in der Ereignisliste
  oder im Repository gespeichert.

Die Abfrage darf den Callmonitor und die Anzeige eingehender Anrufe nicht
blockieren. Änderungen am Telefonbuch können später durch eine erneute Abfrage
übernommen werden; eine dauerhafte Speicherung von Rufnummern oder Namen ist
nicht erforderlich.

## Technologie

- C++20
- CMake
- POSIX-Sockets für den Callmonitor
- D-Bus/libnotify für Desktop-Benachrichtigungen
- Ayatana AppIndicator für das Systemtray
- systemd-User-Service für den automatischen Start

Die Desktop-Bibliotheken werden beim Build optional erkannt. Dadurch bleiben Parser- und Netzwerktests auch auf einem System ohne installierte Tray-Bibliotheken möglich.

## Konfiguration

Die lokale TOML-Datei wird standardmäßig aus `~/.config/fritzmonitor/config.toml`
gelesen. Zugangsdaten werden für den Callmonitor nicht benötigt. Für die
optionale TR-064-Telefonbuchabfrage können dort separate Zugangsdaten hinterlegt
werden. Rufnummern werden nicht dauerhaft gespeichert; die Ereignisliste und
die geladene Telefonbuch-Zuordnung bleiben nur bis zum Programmende im
Speicher.

Beispiel:

```toml
host = "fritz.box"
port = 1012
reconnect_seconds = 5
max_events = 20
notify_incoming = true
notify_missed = true
# Optional: TR-064 phonebook lookup
addressbook_enabled = true
tr064_port = 49000
tr064_username = "fritzmonitor"
tr064_password = "use-a-local-secret"
```

Alternativ können `FRITZMONITOR_TR064_USERNAME` und
`FRITZMONITOR_TR064_PASSWORD` als Umgebungsvariablen verwendet werden. Die
Passwortdatei sollte nur für den Benutzer lesbar sein.

Bei FRITZ!Box-Versionen, die beim Web-Login keinen Benutzernamen verwenden,
bleibt `tr064_username` leer; das Kennwort wird trotzdem an TR-064 übertragen:

```toml
tr064_username = ""
tr064_password = "DEIN_FRITZBOX_PASSWORT"
```

## Kommandozeile und Diagnose

FritzMonitor unterstützt neben dem normalen Start folgende Optionen:

- `--help` zeigt die verfügbaren Optionen.
- `--version` gibt die aus CMake übernommene Projektversion aus.
- `--config PATH` lädt eine alternative TOML-Konfiguration.
- `--simulate` erzeugt einen simulierten eingehenden und anschließend
  verpassten Anruf und beendet sich danach. Damit können Parser-, Konfigurations-
  und grundlegende Desktop-Pfade ohne einen echten Testanruf geprüft werden.

Fehlen die Desktop-Bibliotheken beim Build, wird weiterhin ein Headless-Binary
für Parser- und Netzwerktests erzeugt. Die vollständige Tray-Funktion benötigt
die optional erkannten GTK-, libnotify- und AppIndicator-Bibliotheken.

## Entwicklung

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Der verbindliche Produktiv-Build läuft im Container. Die Entwicklungs-
abhängigkeiten werden nur im Builder-Container installiert; das geprüfte
Binary läuft anschließend nativ auf dem Host mit den dort vorhandenen
Runtime-Bibliotheken:

```sh
./scripts/container-build.sh
```

Die Artefakte werden unter `build/container-release/` abgelegt. Für die
native Desktop-Integration benötigt der Host die Runtime-Pakete für GLib/GIO,
GTK 3, libnotify und Ayatana AppIndicator, aber keine `*-dev`-Pakete.

### Debian- und Fedora-Pakete

CPack stellt zwei explizite CMake-Ziele bereit. Für Pakete wird der systemd-
User-Service auf den systemweiten Installationspfad `/usr/bin/fritzmonitor`
ausgerichtet:

```sh
cmake -S . -B build/packages -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DFRITZMONITOR_SERVICE_EXECUTABLE=/usr/bin/fritzmonitor
cmake --build build/packages --target package-deb
cmake --build build/packages --target package-rpm
```

Beide Ziele bauen das Release-Binary, führen die Tests aus und erzeugen
anschließend das jeweilige Paket. Entwicklungsabhängigkeiten werden dabei
nicht auf dem Produktivsystem benötigt. Für manuell angestoßene Container-
Builds stehen `scripts/package-in-container.sh deb` und `scripts/package-in-
container.sh rpm` bereit. Der GitHub-Workflow verwendet dieselben Wrapper.

Die Container-Wrapper legen Debian-Pakete unter `build/package-deb/` und RPM-
Pakete unter `build/package-rpm/` ab. Debian-Pakete können mit `apt install`
und RPM-Pakete mit `dnf install` installiert werden. Der User-Service wird
danach über `systemctl --user enable --now fritzmonitor.service` aktiviert.

Der GitHub-Workflow `.github/workflows/packages.yml` baut beide Pakettypen bei
jedem Push auf `master` sowie bei manueller Auslösung. Die erzeugten Pakete
werden als Artefakte des jeweiligen Actions-Laufs bereitgestellt; ein Push auf
einen normalen Entwicklerbranch oder ein offener PR löst diesen Workflow nicht
automatisch aus.

Für die vollständige Desktop-Integration werden auf Debian/Ubuntu die oben
genannten Runtime-Bibliotheken benötigt.
