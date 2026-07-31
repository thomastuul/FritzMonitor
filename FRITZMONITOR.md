# FritzMonitor

FritzMonitor soll als native Linux-Systemtray-Anwendung Ereignisse einer FRITZ!Box empfangen und den Benutzer per Desktop-Benachrichtigung informieren.

## MVP

Der erste funktionsfähige Umfang konzentriert sich auf Telefonie-Ereignisse über den FRITZ!Box-Callmonitor auf TCP-Port 1012:

- eingehende Anrufe sofort melden
- angenommene und beendete Anrufe verfolgen
- nicht angenommene Anrufe als verpasste Anrufe melden
- Verbindungsstatus und die letzten Ereignisse im Tray anzeigen
- bei einer Netzwerkunterbrechung automatisch wieder verbinden

Weitere FRITZ!Box-Ereignisse über TR-064/UPnP können später als zusätzliche Adapter ergänzt werden.

## Technologie

- C++20
- CMake
- POSIX-Sockets für den Callmonitor
- D-Bus/libnotify für Desktop-Benachrichtigungen
- Ayatana AppIndicator für das Systemtray
- systemd-User-Service für den automatischen Start

Die Desktop-Bibliotheken werden beim Build optional erkannt. Dadurch bleiben Parser- und Netzwerktests auch auf einem System ohne installierte Tray-Bibliotheken möglich.

## Konfiguration

Die lokale TOML-Datei wird standardmäßig aus `~/.config/fritzmonitor/config.toml` gelesen. Zugangsdaten werden für den Callmonitor nicht benötigt. Rufnummern werden nicht dauerhaft gespeichert; die Ereignisliste bleibt nur bis zum Programmende im Speicher.

Beispiel:

```toml
host = "fritz.box"
port = 1012
reconnect_seconds = 5
max_events = 20
notify_incoming = true
notify_missed = true
```

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

Für die vollständige Desktop-Integration werden auf Debian/Ubuntu die oben
genannten Runtime-Bibliotheken benötigt.
