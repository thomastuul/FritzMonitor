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
