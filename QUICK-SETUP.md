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
2. Für die optionale Telefonbuchauflösung vorzugsweise einen FRITZ!Box-Benutzer
   mit Zugriff auf Sprachnachrichten, Faxnachrichten, FRITZ!App Fon und
   Anrufliste anlegen. Bei Setups mit reiner Kennwortanmeldung kann der
   Benutzername leer bleiben.
3. Den Zugriff für Anwendungen über TR-064 in den Heimnetzfreigaben der
   FRITZ!Box aktivieren. Die genaue Bezeichnung hängt von der FRITZ!OS-Version
   ab.

TR-064 ist die lokale FRITZ!Box-Schnittstelle, über die FritzMonitor bei
aktivierter Telefonbuchauflösung das Telefonbuch abfragt. Mit
**TR-064-Passwort** ist normalerweise das Kennwort des FRITZ!Box-Benutzers
gemeint, dessen Name später unter `tr064_username` eingetragen wird. Bei einer
FRITZ!Box-Anmeldung nur mit dem allgemeinen FRITZ!Box-Kennwort bleibt
`tr064_username` leer; als TR-064-Passwort wird dann dieses allgemeine Kennwort
verwendet. Es handelt sich nicht um den WLAN-Schlüssel und nicht um ein
separates FritzMonitor-Passwort. Der reine Callmonitor auf Port `1012` benötigt
keine Anmeldung; die TR-064-Zugangsdaten werden nur gebraucht, wenn
`addressbook_enabled = true` gesetzt ist.

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
reconnect_max_seconds = 60
allow_nonlocal_addresses = false
max_events = 20
notify_incoming = true
notify_missed = true
addressbook_enabled = true
tr064_port = 49000
tr064_username = "fritzmonitor"
```

Die sichere Voreinstellung akzeptiert für Callmonitor und TR-064 nur private
beziehungsweise lokale IPv4-Adressen sowie IPv6-ULA, Link-Local und Loopback.
Öffentliche und gemischte DNS-Antworten werden vollständig verworfen. Die
geprüfte Adresse wird beim Verbindungsaufbau festgeschrieben; auch die von
TR-064 gelieferte Telefonbuch-URL wird separat geprüft und erhält keine
TR-064-Zugangsdaten. `allow_nonlocal_addresses` sollte deshalb auf `false`
bleiben. Die explizite Ausnahme `true` ist nur für ein anderweitig abgesichertes
Remote- oder VPN-Ziel vorgesehen; TR-064 über HTTP wird dadurch nicht
verschlüsselt.

Bei einer FRITZ!Box-Anmeldung ohne Benutzernamen wird stattdessen Folgendes
eingetragen:

```toml
tr064_username = ""
```

Das allgemeine FRITZ!Box-Kennwort wird anschließend ebenfalls über Schritt 5
gespeichert.

Das Kennwort wird anschließend wie in Schritt 5 beschrieben im Login-Keyring
gespeichert. Falls kein Secret Service beziehungsweise GNOME-Keyring verwendet
werden kann, unterstützt FritzMonitor als Rückfall weiterhin diesen Eintrag:

```toml
tr064_password = "TR064_KENNWORT"
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
Eintrag heißt beispielsweise `FritzMonitor TR-064 (fritz.box)`. FritzMonitor
ordnet ihn dem konfigurierten Host und Benutzernamen zu; ein leerer Benutzername
wird dabei ebenfalls unterstützt.

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

Außerhalb des Heimnetzes bleibt der Dienst absichtlich aktiv. Er schreibt nur
eine Ausfallmeldung und erhöht das Wiederholungsintervall von
`reconnect_seconds` bis höchstens `reconnect_max_seconds`. Nach der Rückkehr
stellt er Callmonitor und eine zuvor fehlgeschlagene Telefonbuchabfrage ohne
Neustart wieder her. Solange keine Verbindung zum Callmonitor besteht, ist das
Telefonhörer-Icon gelb. Nach erfolgreicher Wiederverbindung wird es grün oder,
falls noch ein ungelesener eingehender Anruf vorliegt, rot. Prüfen lässt sich
das mit:

```sh
getent ahosts fritz.box
journalctl --user -u fritzmonitor.service -f
```

Im Heimnetz müssen ausschließlich erwartete lokale Adressen erscheinen; das
Journal meldet anschließend `connected to fritz.box:1012`.

Die installierte Version lässt sich unabhängig vom Dienst prüfen:

```sh
fritzmonitor --version
```

Ausführliche Konfigurations-, Build- und Diagnosehinweise stehen in
[README.md](README.md) und [FRITZMONITOR.md](FRITZMONITOR.md).
