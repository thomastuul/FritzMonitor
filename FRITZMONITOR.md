# FritzMonitor

## Version und Copyright

FritzMonitor verwendet das Schema `MAJOR.MINOR.PATCH` (SemVer). Die alleinige
Versionsquelle ist die Datei `VERSION` im Projektroot; CMake übernimmt diesen
Wert für Binary und Pakete. Debian- und RPM-Pakete verwenden zusätzlich ein
Paket-Release, derzeit `1`. Das Projekt wurde erstellt und gestaltet von Thomas
Tuul zusammen mit OpenAI Codex. Die vollständige Attribution steht in
`COPYRIGHT.md`. FritzMonitor wird unter der GNU General Public License Version 3
(GPLv3) veröffentlicht; siehe `LICENSE`.

FritzMonitor soll als native Linux-Systemtray-Anwendung Ereignisse einer
FRITZ!Box empfangen und den Benutzer per Desktop-Benachrichtigung informieren.

## MVP

Der erste funktionsfähige Umfang konzentriert sich auf Telefonie-Ereignisse über
den FRITZ!Box-Callmonitor auf TCP-Port 1012:

- eingehende Anrufe sofort melden
- angenommene und beendete Anrufe verfolgen
- nicht angenommene Anrufe als verpasste Anrufe melden
- Verbindungsstatus und die letzten Ereignisse im Tray anzeigen
- bei einer Netzwerkunterbrechung automatisch wieder verbinden

Weitere FRITZ!Box-Ereignisse über TR-064/UPnP können später als zusätzliche
Adapter ergänzt werden.

## Desktop-Benachrichtigungen

FritzMonitor verwendet die `libnotify`-Bibliothek direkt. Die Benachrichtigung
wird über D-Bus an den Desktop-Benachrichtigungsdienst gesendet; ein
installiertes `notify-send` ist nicht erforderlich. `notify-send` und
FritzMonitor verwenden damit dieselbe standardisierte
Benachrichtigungsschnittstelle.

Die Meldungen verwenden den Anwendungsnamen `FritzMonitor`, das installierte
Telefon-Icon, die Kategorie `phone.call`, normale Dringlichkeit und eine
Anzeigedauer von fünf Sekunden. Wenn keine D-Bus-Benachrichtigungsinstanz
verfügbar ist, läuft FritzMonitor ohne Desktop-Meldungen weiter und
protokolliert dies im User-Journal. Ein Shell-Aufruf von `notify-send` wird
nicht als Fallback verwendet.

## Tray-Icon und Anrufstatus

Das Systemtray verwendet ein telefonförmiges FritzMonitor-Icon mit zwei
Zuständen:

- Grün: Seit dem Start des Monitors ist kein eingehender Anruf eingegangen oder
  alle eingegangenen Anrufe wurden durch das Öffnen des Pulldown-Menüs
  abgefragt.
- Rot: Seit dem Start ist mindestens ein eingehender Anruf eingegangen, der noch
  nicht durch das Öffnen des Pulldown-Menüs abgefragt wurde.

Das Öffnen des Pulldown-Menüs bestätigt den aktuellen Anrufstatus und setzt das
Icon wieder auf Grün. Die grünen und roten SVG-Icons werden zusammen mit der
nativen Desktop-Binärdatei installiert.

Das Pulldown-Menü zeigt pro Anruf genau eine Zeile in dieser Reihenfolge:

1. grüner Telefonhörer bei einem angenommenen Anruf oder roter Telefonhörer bei
   einem verpassten Anruf
2. Datum und Uhrzeit im Format `12.07. 18:23`
3. Name oder, falls kein Name zugeordnet werden kann, die Rufnummer

Eine Namensauflösung kann optional über das FRITZ!Box-Adressbuch erfolgen; wenn
keine Auflösung verfügbar ist, darf der Anruf trotzdem nicht verworfen werden.
Es werden immer nur die letzten drei Anrufe angezeigt, der neueste Eintrag steht
oben. Bei deutscher Systemlokalisierung sind Menütexte und Tooltips deutsch,
ansonsten englisch.

## Namensauflösung über das FRITZ!Box-Adressbuch

Die Namensauflösung über das FRITZ!Box-Adressbuch ist eine optionale
Erweiterung. FritzMonitor verwendet dafür die lokale TR-064/UPnP-Schnittstelle
der FRITZ!Box, insbesondere den Telefonbuchdienst `X_AVM-DE_OnTel`.

- Alle über TR-064 verfügbaren Telefonbücher werden beim Programmstart geladen
  und gemeinsam im Speicher gehalten. Scheitert das unterwegs, wird die Abfrage
  mit begrenztem Backoff wiederholt und nach der Rückkehr ins Heimnetz ohne
  Neustart nachgeholt.
- Rufnummern werden vor dem Vergleich normalisiert, damit unterschiedliche
  Schreibweisen wie `+49...` und `0...` zugeordnet werden können.
- Für einen Treffer wird der Name zusätzlich zur Rufnummer im Pulldown-Menü
  angezeigt.
- Ist die FRITZ!Box nicht erreichbar, TR-064 deaktiviert oder die Abfrage nicht
  berechtigt, bleibt FritzMonitor funktionsfähig und zeigt die Rufnummer an.
- Das TR-064-Passwort wird bevorzugt über den Secret Service des Desktops
  bezogen und kann beispielsweise mit Seahorse verwaltet werden. Es wird nicht
  in der Ereignisliste oder im Repository gespeichert.

## Netzwerk-Vertrauensgrenze

Callmonitor und TR-064 verwenden dieselbe fail-closed Zielprüfung. In der
Voreinstellung sind ausschließlich diese Adressklassen zulässig:

- IPv4: `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16`, Link-Local
  `169.254.0.0/16` und Loopback `127.0.0.0/8`
- IPv6: ULA `fc00::/7`, Link-Local `fe80::/10` und Loopback `::1`
- IPv4-gemappte IPv6-Adressen nur dann, wenn die enthaltene IPv4-Adresse nach
  denselben Regeln zulässig ist

Multicast, unspecified, Carrier-Grade-NAT- und öffentlich routbare Adressen
werden abgewiesen. Enthält eine DNS-Antwort sowohl erlaubte als auch unerlaubte
Adressen, wird die gesamte Antwort verworfen. Dadurch kann keine von mehreren
Adressen unbemerkt die Sicherheitsentscheidung umgehen.

Der Callmonitor verbindet den POSIX-Socket direkt mit den bereits geprüften
`sockaddr`-Werten. Für HTTP(S) trägt FritzMonitor denselben DNS-Snapshot mit
`CURLOPT_RESOLVE` in den libcurl-Transfer ein. Damit gibt es zwischen Prüfung
und Nutzung keine zweite DNS-Auflösung. Systemweite HTTP-Proxys, `.netrc`,
Redirects und andere URL-Protokolle werden für diese Requests deaktiviert.

TR-064-Zugangsdaten werden ausschließlich für die fest konstruierte
`/upnp/control/x_contact`-URL gesetzt. Eine vom Endpunkt gelieferte
Telefonbuch-URL muss ein explizites `http`- oder `https`-Schema und
ausschließlich erlaubte Zieladressen besitzen. Sie wird ohne TR-064-Benutzername
und -Passwort abgerufen. Diese Trennung verhindert, dass ein manipulierter
SOAP-Inhalt die Zugangsdaten an einen weiteren Host weiterreicht. Das lokale
HTTP-Protokoll selbst bietet allerdings keine kryptografische
Serverauthentisierung; ein Angreifer im zugelassenen lokalen Netz bleibt deshalb
ein Restrisiko.

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
- libsecret für den verschlüsselten Credential-Speicher
- systemd-User-Service für den automatischen Start

Die Desktop-Bibliotheken werden beim Build optional erkannt. Dadurch bleiben
Parser- und Netzwerktests auch auf einem System ohne installierte
Tray-Bibliotheken möglich.

## Konfiguration

Die lokale TOML-Datei wird standardmäßig aus
`~/.config/fritzmonitor/config.toml` gelesen. Zugangsdaten werden für den
Callmonitor nicht benötigt. Für die optionale TR-064-Telefonbuchabfrage wird
dort nur der Benutzername hinterlegt; das Passwort liegt standardmäßig im Secret
Service. Rufnummern werden nicht dauerhaft gespeichert; die Ereignisliste und
die geladene Telefonbuch-Zuordnung bleiben nur bis zum Programmende im Speicher.

Beispiel:

```toml
host = "fritz.box"
port = 1012
reconnect_seconds = 5
reconnect_max_seconds = 60
allow_nonlocal_addresses = false
max_events = 20
notify_incoming = true
notify_missed = true
# Optional: TR-064 phonebook lookup
addressbook_enabled = true
tr064_port = 49000
tr064_username = "fritzmonitor"
```

`reconnect_seconds` bestimmt die erste Pause nach einem Fehlschlag. Das
Intervall verdoppelt sich bis `reconnect_max_seconds` und wird nach
erfolgreicher Verbindung zurückgesetzt. Eine zusammenhängende Ausfallphase
erzeugt nur eine Statusmeldung. Bei den Standardwerten wird ein Netzwechsel
zurück ins Heimnetz spätestens nach rund 60 Sekunden zuzüglich Auflösungs- und
Verbindungszeit erkannt.

`allow_nonlocal_addresses = true` ist eine explizite, standardmäßig deaktivierte
Ausnahme für bewusst eingerichtete Remote- oder VPN-Nutzung. Sie erlaubt auch
öffentlich routbare Auflösungen, behält aber DNS-Pinning, Protokollgrenzen und
Credential-Trennung bei. Da der fest konfigurierte TR-064-Endpunkt weiterhin
unverschlüsseltes HTTP verwendet, darf die Ausnahme nicht als sichere
Internetfreigabe verstanden werden; sie setzt ein vertrauenswürdiges,
anderweitig geschütztes Zielnetz voraus.

Das Passwort wird im Default-Keyring unter dem Schema
`org.fritzmonitor.Tr064Credentials` und den Attributen Anwendung, Host und
Benutzername gespeichert. Der sichtbare Eintrag heißt beispielsweise
`FritzMonitor TR-064 (fritz.box)`. Dadurch kann Seahorse den Eintrag anzeigen,
ändern oder löschen. Der Login-Keyring muss in der grafischen Benutzersitzung
entsperrt sein.

Ein vorhandenes Klartext-Passwort wird einmalig sicher migriert:

```sh
fritzmonitor --migrate-tr064-password
```

FritzMonitor speichert zuerst das Passwort im Secret Service, liest es zur
Kontrolle zurück und ersetzt erst danach die Konfigurationsdatei atomar durch
eine Fassung ohne `tr064_password`. Scheitert das Speichern oder die Prüfung,
bleibt die ursprüngliche Konfiguration unverändert. Ein neues oder in Seahorse
nicht direkt geändertes Passwort kann verdeckt eingegeben werden:

```sh
fritzmonitor --store-tr064-password
```

Für die Abwärtskompatibilität wird ein vorhandenes `tr064_password` in der
TOML-Datei weiterhin akzeptiert. Ist dort kein Passwort gesetzt, folgt
`FRITZMONITOR_TR064_PASSWORD`; erst danach wird der Secret Service abgefragt.
`FRITZMONITOR_TR064_USERNAME` kann einen leeren TOML-Benutzernamen ergänzen.
Eine Passwortdatei oder eine systemd-`EnvironmentFile` wäre dagegen wieder eine
Klartextablage und wird nicht empfohlen.

Bei FRITZ!Box-Versionen, die beim Web-Login keinen Benutzernamen verwenden,
bleibt `tr064_username` leer; das Kennwort wird trotzdem an TR-064 übertragen:

```toml
tr064_username = ""
```

## Kommandozeile und Diagnose

FritzMonitor unterstützt neben dem normalen Start folgende Optionen:

- `--help` zeigt die verfügbaren Optionen.
- `--version` gibt die aus CMake übernommene Projektversion aus.
- `--config PATH` lädt eine alternative TOML-Konfiguration.
- `--simulate` erzeugt einen simulierten eingehenden und anschließend verpassten
  Anruf und beendet sich danach. Damit können Parser-, Konfigurations- und
  grundlegende Desktop-Pfade ohne einen echten Testanruf geprüft werden.
- `--store-tr064-password` liest ein neues Passwort zweimal mit deaktivierter
  Terminalanzeige und speichert es im Secret Service.
- `--migrate-tr064-password` übernimmt ein vorhandenes TOML-Passwort in den
  Secret Service und entfernt den Klartext erst nach erfolgreicher Prüfung.

Außerhalb des Heimnetzes ist eine einzelne Journalmeldung mit
`untrusted address` normal, sofern `fritz.box` dort öffentlich aufgelöst wird.
Der Dienst bleibt aktiv und prüft mit Backoff weiter. Diagnose und Rückkehrtest:

```sh
getent ahosts fritz.box
systemctl --user status fritzmonitor.service
journalctl --user -u fritzmonitor.service -n 30 --no-pager
journalctl --user -u fritzmonitor.service -f
```

Unterwegs darf keine `connected to`-Meldung für eine öffentliche Adresse folgen.
Nach dem Wechsel ins Heimnetz müssen `getent` ausschließlich erlaubte lokale
Adressen und das Journal innerhalb des maximalen Intervalls
`connected to fritz.box:1012` zeigen. Für den positiven Livetest sollte
anschließend ein Testanruf erfolgen; bei aktivierter Telefonbuchfunktion muss
zudem `loaded ... phonebooks` erscheinen und der Name im Tray aufgelöst werden.

Fehlen die Desktop-Bibliotheken beim Build, wird weiterhin ein Headless-Binary
für Parser- und Netzwerktests erzeugt. Die vollständige Tray-Funktion benötigt
die optional erkannten GTK-, libnotify- und AppIndicator-Bibliotheken.

## Entwicklung

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Der verbindliche Produktiv-Build läuft im Container. Die
Entwicklungsabhängigkeiten werden nur im Builder-Container installiert; das
geprüfte Binary läuft anschließend nativ auf dem Host mit den dort vorhandenen
Runtime-Bibliotheken:

```sh
./scripts/container-build.sh
```

Die Artefakte werden unter `build/container-release/` abgelegt. Für die native
Desktop-Integration benötigt der Host die Runtime-Pakete für GLib/GIO, GTK 3,
libnotify, Ayatana AppIndicator und libsecret, aber keine `*-dev`-Pakete.

### Debian- und Fedora-Pakete

CPack stellt zwei explizite CMake-Ziele bereit. Für Pakete wird der
systemd-User-Service auf den systemweiten Installationspfad
`/usr/bin/fritzmonitor` ausgerichtet:

```sh
cmake -S . -B build/packages -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DFRITZMONITOR_SERVICE_EXECUTABLE=/usr/bin/fritzmonitor
cmake --build build/packages --target package-deb
cmake --build build/packages --target package-rpm
```

Beide Ziele bauen das Release-Binary, führen die Tests aus und erzeugen
anschließend das jeweilige Paket. Entwicklungsabhängigkeiten werden dabei nicht
auf dem Produktivsystem benötigt. Für manuell angestoßene Container-Builds
stehen `scripts/package-in-container.sh deb` und
`scripts/package-in-container.sh rpm` bereit. Der GitHub-Workflow verwendet
dieselben Wrapper.

Die Container-Wrapper legen Debian-Pakete unter `build/package-deb/` und
RPM-Pakete unter `build/package-rpm/` ab. Debian-Pakete können mit `apt install`
und RPM-Pakete mit `dnf install` installiert werden. Der User-Service wird
danach über `systemctl --user enable --now fritzmonitor.service` aktiviert.

Der GitHub-Workflow `.github/workflows/packages.yml` baut beide Pakettypen bei
jedem Push auf `master` sowie bei manueller Auslösung. Die erzeugten Pakete
werden als Artefakte des jeweiligen Actions-Laufs bereitgestellt; ein Push auf
einen normalen Entwicklerbranch oder ein offener PR löst diesen Workflow nicht
automatisch aus. Wird ein Tag im Format `v<SemVer>` gepusht, baut der Workflow
die Pakete zusätzlich und erstellt daraus automatisch einen GitHub-Release mit
beiden Paketdateien als Release-Assets. Vor dem Veröffentlichen wird geprüft,
dass die Paketversion zum Tag passt:

```sh
version=$(cat VERSION)
git tag -a "v$version" -m "FritzMonitor $version"
git push origin "v$version"
```

Für die vollständige Desktop-Integration werden auf Debian/Ubuntu die oben
genannten Runtime-Bibliotheken einschließlich `libsecret-1-0` benötigt. Fedora
stellt die entsprechende Runtime im Paket `libsecret` bereit.
