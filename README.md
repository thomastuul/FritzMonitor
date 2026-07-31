# FritzMonitor

FritzMonitor ist ein nativer Linux-Systemtray-Monitor für den FRITZ!Box-
Callmonitor. Er verbindet sich über TCP mit Port `1012`, meldet eingehende
Anrufe per Desktop-Benachrichtigung und zeigt den Anrufstatus im Tray.

Version `0.2.0` ist der aktuelle funktionale Entwicklungsstand. FritzMonitor
wurde erstellt und gestaltet von Thomas Tuul zusammen mit OpenAI Codex.

![Systemtray mit FritzMonitor](docs/images/fritzmonitor-systemtray.png)

Das telefonförmige Icon ist grün, solange keine ungelesenen eingehenden Anrufe
vorliegen. Nach einem eingehenden Anruf wird es rot; das Öffnen des Menüs
markiert die Anrufe als gelesen.

## Pulldown-Menü

Das Menü enthält pro Anruf genau eine Zeile. Angezeigt werden Rufnummer,
Uhrzeit und Status. Es bleiben die letzten drei Anrufe erhalten, der neueste
steht oben. Namen werden angezeigt, sobald eine Namensauflösung verfügbar ist;
ansonsten bleibt die Rufnummer sichtbar.

![Beispielansicht des FritzMonitor-Menüs mit anonymisierten Nummern](docs/images/fritzmonitor-menu-example.svg)

Die Menü- und Statussprache folgt der Systemlokalisierung: Deutsch bei einer
deutschen Locale, sonst Englisch.

## Verwendung

Der FRITZ!Box-Callmonitor muss aktiviert sein. Dazu auf einem angeschlossenen
Telefon `#96*5*` wählen. FritzMonitor verbindet sich anschließend automatisch
mit `fritz.box:1012` beziehungsweise dem in der Konfiguration angegebenen
Host und verbindet sich nach Netzwerkunterbrechungen erneut.

Die Konfiguration liegt standardmäßig unter:

```text
~/.config/fritzmonitor/config.toml
```

```toml
host = "fritz.box"
port = 1012
reconnect_seconds = 5
max_events = 20
notify_incoming = true
notify_missed = true
```

## Installation und Pakete

Die Paketversion folgt [SemVer](https://semver.org/): `MAJOR.MINOR.PATCH` für
API-/Funktionsänderungen und Fehlerkorrekturen. Debian- und RPM-Pakete tragen
zusätzlich ein distributionsspezifisches Paket-Release, derzeit `1`.

Nach einer CMake-Konfiguration mit den Paketpfaden können die Pakete manuell
gebaut werden:

```sh
cmake -S . -B build/packages -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DFRITZMONITOR_SERVICE_EXECUTABLE=/usr/bin/fritzmonitor
cmake --build build/packages --target package-deb
cmake --build build/packages --target package-rpm
```

Die erzeugten Dateien liegen unter `build/packages/`. Für reproduzierbare
Builds ohne Entwicklungssoftware auf dem Host stehen Container-Wrapper bereit:

```sh
./scripts/package-in-container.sh deb
./scripts/package-in-container.sh rpm
```

Der produktive Native-Build ohne Paketierung bleibt:

```sh
./scripts/container-build.sh
```

Die geprüften Artefakte liegen danach unter `build/container-release/`. Eine
vollständige technische Beschreibung steht in [FRITZMONITOR.md](FRITZMONITOR.md).

GitHub Actions baut Debian- und Fedora-Pakete bei manueller Auslösung sowie bei
Pushes auf `master` und legt sie als Workflow-Artefakte ab. Die Attribution
steht in [COPYRIGHT.md](COPYRIGHT.md).

## Entwicklung

Der Build verwendet C++20 und CMake. Entwicklungsabhängigkeiten werden nur im
Builder-Container installiert; auf dem Produktivsystem werden ausschließlich
Runtime-Bibliotheken benötigt. Der Container-Build führt auch die Tests aus.
