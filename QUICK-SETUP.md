# FritzMonitor Quick Setup

Diese Kurzfassung installiert das fertige Debian-Paket, richtet den
FRITZ!Box-Zugriff ein und speichert das TR-064-Passwort im GNOME-Keyring.

## 1. Voraussetzungen installieren

Docker oder Entwicklungswerkzeuge werden für ein fertiges Debian-Paket nicht
benötigt. Seahorse und der GNOME-Keyring werden mit den Debian-Paketen
installiert:

```sh
sudo apt update
sudo apt install seahorse gnome-keyring
```

## 2. Debian-Paket installieren

Im Verzeichnis mit dem heruntergeladenen Paket:

```sh
sudo apt install ./fritzmonitor-*-Linux.deb
```

Wer das Paket aus dem Quellcode erzeugt, verwendet stattdessen:

```sh
./scripts/package-in-container.sh deb
sudo apt install "./build/package-deb/fritzmonitor-$(cat VERSION)-Linux.deb"
```

## 3. FRITZ!Box vorbereiten

1. Auf einem an der FRITZ!Box angemeldeten Telefon `#96*5*` wählen, um den
   Callmonitor zu aktivieren.
2. Für die optionale Telefonbuchauflösung einen FRITZ!Box-Benutzer mit Zugriff
   auf Sprachnachrichten, Faxnachrichten, FRITZ!App Fon und Anrufliste anlegen.
3. Den Zugriff für Anwendungen über TR-064 in den Heimnetzfreigaben der
   FRITZ!Box aktivieren. Die genaue Bezeichnung hängt von der FRITZ!OS-Version
   ab.

## 4. Konfiguration anlegen

```sh
install -d -m 700 ~/.config/fritzmonitor
editor ~/.config/fritzmonitor/config.toml
chmod 600 ~/.config/fritzmonitor/config.toml
```

Minimalbeispiel ohne Klartext-Passwort:

```toml
host = "fritz.box"
port = 1012
reconnect_seconds = 5
max_events = 20
notify_incoming = true
notify_missed = true
addressbook_enabled = true
tr064_port = 49000
tr064_username = "fritzmonitor"
```

## 5. Passwort mit Seahorse einrichten

Der Login-Keyring muss in der grafischen Sitzung entsperrt sein. Das Passwort
wird verdeckt zweimal abgefragt und anschließend über den Secret Service
gespeichert:

```sh
fritzmonitor --store-tr064-password
```

Danach Seahorse öffnen, den Bereich **Passwörter** beziehungsweise den
**Login**-Schlüsselbund wählen und nach `FritzMonitor TR-064` suchen. Der
Eintrag heißt beispielsweise `FritzMonitor TR-064 (fritz.box)`.

Enthält eine ältere Konfiguration noch `tr064_password`, wird sie stattdessen
einmalig migriert:

```sh
fritzmonitor --migrate-tr064-password
```

FritzMonitor entfernt die Klartextzeile erst, nachdem das Passwort gespeichert
und erfolgreich zurückgelesen wurde.

## 6. Dienst starten und prüfen

```sh
systemctl --user daemon-reload
systemctl --user enable --now fritzmonitor.service
systemctl --user status fritzmonitor.service
journalctl --user -u fritzmonitor.service -n 30 --no-pager
```

Die installierte Version lässt sich unabhängig vom Dienst prüfen:

```sh
fritzmonitor --version
```

Ausführliche Konfigurations-, Build- und Diagnosehinweise stehen in
[README.md](README.md) und [FRITZMONITOR.md](FRITZMONITOR.md).
