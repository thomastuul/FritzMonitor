# AGENTS.md

## Build und Installation

- FritzMonitor wird grundsätzlich in einem Container gebaut. Die erforderlichen
  Compiler-, Header- und Entwicklungsbibliotheken dürfen dafür im Builder-
  Container installiert werden, nicht auf dem Produktivsystem.
- Das erzeugte Binary läuft anschließend nativ auf dem Host. Auf dem Host dürfen
  dafür ausschließlich die benötigten Runtime-Bibliotheken installiert werden;
  eine Ausführung des Programms in einem Container ist nicht der Standardweg.
- Der Container-Build muss neben dem Release-Binary auch die vorhandenen Tests
  ausführen. Vor einer Installation sind Build, Tests und die tatsächliche
  Binary-Ausgabe zu verifizieren.
- Änderungen an Container-Build, Runtime-Abhängigkeiten oder Installationspfad
  sind in dieser Datei und in der Projektdokumentation konsistent zu halten.

## Automatische Versionierung

- Die Projektversion wird automatisch nach Semantic Versioning angepasst; eine
  manuelle Änderung der Versionsnummer in einzelnen Dateien ist zu vermeiden.
- Ein Bugfix erhöht die PATCH-Komponente (zum Beispiel `0.2.0` → `0.2.1`).
- Ein rückwärtskompatibles Feature erhöht die MINOR-Komponente (zum Beispiel
  `0.2.1` → `0.3.0`) und setzt PATCH auf `0`.
- Eine inkompatible Änderung erhöht die MAJOR-Komponente (zum Beispiel `0.3.0`
  → `1.0.0`) und setzt MINOR und PATCH auf `0`.
- Die automatische Einstufung erfolgt anhand des Änderungstyps im Commit bzw.
  Pull Request; bei mehreren Änderungstypen gilt die höchste Stufe
  `breaking change` > `feature` > `bugfix`.
- Die ermittelte Version muss in CMake, Binary-Ausgabe, Debian-Paket und RPM-
  Paket konsistent verwendet werden. Das distributionsspezifische Paket-
  Release wird unabhängig von der SemVer-Quelle fortlaufend erhöht, wenn
  derselbe Quellstand erneut paketiert wird.
