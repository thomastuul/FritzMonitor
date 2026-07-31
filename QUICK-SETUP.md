# FritzMonitor Quick Setup

Diese Kurzfassung installiert das fertige Debian-Paket, richtet den
FRITZ!Box-Zugriff ein und speichert das TR-064-Passwort vorzugsweise im
GNOME-Keyring. Dadurch steht es nicht im Klartext in einer Konfigurationsdatei.

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

TR-064 ist die lokale FRITZ!Box-Schnittstelle, über die FritzMonitor bei
aktivierter Telefonbuchauflösung das Telefonbuch abfragt. Mit
**TR-064-Passwort** ist das Kennwort des FRITZ!Box-Benutzers gemeint, dessen
Name später unter `tr064_username` eingetragen wird. Es handelt sich nicht um
den WLAN-Schlüssel und nicht um ein separates FritzMonitor-Passwort. Der reine
Callmonitor auf Port `1012` benötigt keine Anmeldung; Benutzername und Kennwort
werden nur gebraucht, wenn `addressbook_enabled = true` gesetzt ist.

## 4. Konfiguration anlegen

```sh
install -d -m 700 ~/.config/fritzmonitor
editor ~/.config/fritzmonitor/config.toml
chmod 600 ~/.config/fritzmonitor/config.toml
```

Empfohlen wird eine Konfiguration ohne Klartext-Passwort:

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

Das Kennwort wird anschließend wie in Schritt 5 beschrieben im Login-Keyring
gespeichert. Falls kein Secret Service beziehungsweise GNOME-Keyring verwendet
werden kann, unterstützt FritzMonitor als Rückfall weiterhin diesen Eintrag:

```toml
tr064_password = "KENNWORT_DES_FRITZBOX_BENUTZERS"
```

Diese Variante speichert das Kennwort im Klartext in
`~/.config/fritzmonitor/config.toml`. Die Dateiberechtigung `0600` beschränkt
den Zugriff zwar auf den eigenen Benutzer, ersetzt aber keine geschützte
Passwortverwaltung. Ist `tr064_password` in der Datei gesetzt, verwendet
FritzMonitor diesen Wert direkt und fragt den Keyring nicht ab.

## 5. Passwort vorzugsweise mit Seahorse einrichten

Seahorse ist die grafische Verwaltung für den GNOME-Keyring. FritzMonitor
speichert das Kennwort über den Secret Service im Login-Keyring, sodass es nicht
als Klartext in `config.toml` stehen muss. Diese Variante ist gegenüber dem
Konfigurations-Fallback zu bevorzugen.

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
