# MixEngine V3 — technischer Audit-Zwischenstand / Übergabe

**Status: AUDIT NOT PASSED. Keine FINAL-Freigabe.** Stand 15.10.2026. Der Windows-Nachweis, vollständige native Bedienprüfung, reale Audioreferenz und Studio-One-Abnahme fehlen. Die unten dokumentierten Reparaturen sind lokal überprüft; das ist keine vollständige Produktfreigabe.

## 1. FULL-Ausgangsstand und Demo

- Maßgebliches Repository: `challanger2000/125A-MixEngine-Final`.
- Vom Produktverantwortlichen bestätigter FULL-Stand: V3.0.0, `v3.0.0-development`, Ausgangs-HEAD `23b4ddb669f02dbeb780a87a997522d98c7f426c`.
- GitHub-Webseite des [Windows-Laufs 37497801176](https://github.com/challanger2000/125A-MixEngine-Final/actions/runs/37497801176) nennt genau diesen Commit, `Success`, Job `112386732057`, ein 24.9-MB-Artefakt `125A-MixEngine-V3-v3.0.0-Windows-x64-Gumroad` und Digest `c83dbaac0d6ec48857e4929ab08c34947e4a8ab3b567d06bba474f34834e9252`. Vollständige Joblogs und Binärinhalt konnten nicht heruntergeladen/geprüft werden. Das ist Metadaten-Evidenz, kein unabhängiger Binär-PASS.
- Der Nutzer bezeichnet sein Paket als `...Gumroad-FINAL`; die vorhandene Pipeline/Artefaktmetadaten verwenden `...Gumroad`. Diese Benennungen nicht ohne Prüfung des ZIP-Inhalts gleichsetzen.
- Keine Tags oder GitHub Releases gefunden; die öffentliche Release-Seite meldet keine Releases. Kein CURRENT-/Release-Manifest vorhanden. `main` ist noch V1.1.0. Die Engineering-Rollout-Liste ist laut Nutzer veraltet.
- Demo-HEAD `541f427519e53174a0ea9f751ea2c591b342b4eb` basiert auf V2 (`0055d0bbe05ef9502a71e1c81fca3b56ac6930c0`), nicht auf dem späteren V3-DSP. Die V3-Metadaten dort ersetzen keinen V3-Quellstand; eine Gesamtübernahme würde die V3-Modelle und zahlreiche Tests entfernen.
- Demo-Commits wurden auf LicenseStatus, Produktidentität, gemeinsame DemoTimeline, Einbindung in beide Audio-Pfade, Badge und Testregistrierung untersucht. Der separate originale DemoTimeline-Test besteht im Release-Build (`evidence/demo-timeline.txt`). Windows-Lizenzdatei-Auflösung, ungültige/fremde Lizenzen und echte Demo-/Full-Binärgleichheit: **NOT VERIFIED**. Der freigegebene V3-Quellzweig enthält diese Lizenzintegration nicht; sie wurde nicht als neue Produktfunktion übernommen.
- Die Mac-Abzweigung enthält eine anwendbare Alpha-Typkorrektur und eine Korrektur der veralteten Tube-Routing-Prüfung. Diese wurden gezielt übernommen bzw. am aktuellen V3-Code korrigiert; kein Branch wurde pauschal gemergt.

## 2. Bestätigte Fehler und Reparaturen

| Fehler | Ursache / Beweis | Reparatur und Rückprüfung |
|---|---|---|
| Globaler Bypass friert die Wet-DSP ein | Frühzeitiger Return; Wiederaufnahme-Delta 0.372306 gegenüber kontinuierlicher Referenz | Wet-Pfad kontinuierlich, eigener Dry-Delay, 5-ms-Überblendung; nach Überblendung Delta **0**, Dry-Identität **0**; beide Pfade und tatsächlich kompilierter Channel |
| Bypass benutzt gemeinsame Dry-/Wet-Delay-Historie | Bypass schreibt Rohsignal in den sonstigen Wet-Ausgangspuffer | Eigene ständig laufende Dry-Aligner; feste Hostlatenz und Bypass-Impuls-Matrix unverändert bestanden |
| Tail fälschlich 0 | SDK-Default trotz Nachlauf über viele Tausend Samples | Ratenabhängige konservative 2-s-Meldung; aktive Rauschgeneratoren unendlicher Tail; Impuls/Silence-Nachlauf geprüft |
| Beschädigte States ändern Parameter teilweise | Direkte Zuweisungen vor vollständiger Stream-Prüfung | Gemeinsamer temporärer Decoder für Processor und Controller; Zurückweisung ohne Mutation |
| Teilweise optionale Doubles werden als Legacy-EOF akzeptiert | Fehlgeschlagener readDouble unterscheidet 0 und 1…7 gelesene Bytes nicht | Nur leere optionale Felder gelten als Legacy-Ende; 14 beschädigte Längen negativ geprüft; fünf vollständige Längen erhalten |
| VST3-Rampen werden fälschlich als Sprünge verarbeitet | IParamValueQueue beschreibt lineare Segmente mit implizitem Vorblockwert bei Offset −1; vorheriger Width-Rampenfehler **0.196875** | Gemeinsame Segmentinterpolation im normalen Pfad, Mix-FX-Kanal und Console-Coupling; double-Fehler **0**, float ≤1.78814e−8; Blockpartition 64/16/1; echte Sprünge mit benachbarten Alt-/Neu-Punkten bleiben exakt |
| OUTPUT-Automation springt abrupt | Direkter Multiplikator statt geglättetem linearem Gain | 5-ms-Zeitkonstante, Initialisierung aus Start-/Restore-Wert; DC-Sprung **0.596214 → 0.00247906**, hergeleitete Grenze eingehalten |
| setProcessing macht große Reinitialisierung im möglichen Audiothread | Rekonstruktion aller DSP-Bänke/Oversampler; etwa 2.4 ms in lokalen Messungen | Vorbereitung in Setup/Activation/State; Pause/Resume hält Zustand warm; gemessen etwa 0.03–0.16 µs, ohne Gleichsetzung mit Windows-Hosttiming |
| Große endliche Inputs erzeugen NaN und vergiften Zustände | Nur NaN/Inf wurden am Eingang geprüft; Folgearithmetik läuft über | Werte oberhalb 120 dBFS als ungültig/Null behandeln; keine riesige geklemmte Ersatzimpuls-Injektion; ±DBL_MAX nun endlich und Peak <1 im Prüffall |
| CLIP-Telemetrie kann eigene Texte nicht zurückübersetzen | Einfacher stepCount-1-Parameter liefert Off/On, numerischer Parser lehnt ab; Validator blieb grün | Read-only StringListParameter mit gleichen IDs/Defaults/Schritten; echte DLL-Controller-Konvertierung aller Endpunkte besteht |
| V3-Parameter-Contract erwartet alten Tube-Kern | Veralteter statischer Suchstring | Tatsächlichen V3-Wrapper und beide Aufrufpfade prüfen |
| Veralteter GUI-Prüfpfad widerspricht freigegebenen V3-Ressourcen | verify_gui_layout verlangt frühere prozedurale Generatorausgabe ohne Bitmaps und ohne 90%-Zoom | Prüfung an maßgebliche XML-/Achsenprüfung, vorhandene Atlas-/3x-Bindings und fünf reale Zoomfaktoren angepasst; Oberfläche unverändert |
| GNU-Testbuild scheitert an finite(double) | Kollidiert mit libc-Namensraum | Testhelper zu isFinite umbenannt; Release-Diagnosen gebaut |
| Nichtportable CColor-Alpha-Initialisierung | unsigned-Conditional wird nach uint8 verengt | Explizite uint8-Konvertierung aus dem Mac-Korrekturstand; tatsächlicher GNU-Pluginbuild besteht |

Keine neuen ParamIDs, UIDs, Versionsnummern, Release-Tags oder kommerziellen Lizenztexte. Processor-State bleibt 30 little-endian Doubles / 240 Bytes. Vollständige 26/27/28/29/30-Slot-States werden weiterhin gelesen. Intentionale V3-Grundfärbung bei 0 % und programmeabhängiges Level Match bleiben erhalten. README/Handbuchbehauptungen über exakte Live-0%-Neutralität und ausschließlich feste Kompensation sind gegenüber der tatsächlichen V3-Historie veraltet; das wurde nicht als Anlass für eine Klangänderung genutzt.

## 3. Ausgeführte Tests

- **56/56** ausgewählte DSP-/Processor-/State-Diagnosen im GNU Release-Build bestanden; Liste und Ausgaben: `evidence/final-regressions.txt` und Build-Testquellen.
- Unabhängige Width-Rampen-/Sprungreferenz gemäß gepinntem SDK `pluginterfaces/vst/ivstparameterchanges.h`, Float/Double und Blockpartition 64/16/1: `automation-curve-contract` und `automation-curve-channel-contract`; Vorher-/Nachher-Evidenz enthalten. Alte absichtliche Sprungfixtures wurden mit Altwert bei n−1 und Neuwert bei n korrekt als Hostqueue kodiert; ihre Audio-Grenzen wurden nicht gelockert.
- Tatsächlich mit `MIXENGINE_CHANNEL_BUILD=1` kompilierte zusätzliche Host-/State-Regression; Mix FX und normaler Pfad separat. `audit_host_contract.cpp` enthält zwingend aktive Negativtests ohne assert/NDEBUG-Abhängigkeit.
- Tatsächlich geladenes Channel-VST3: Controller x32 Initialize/Terminate, Parameter-Metadaten, Endpunkt-Textkonvertierung, atomare Fehlerfälle und fünf Legacy-Längen bestanden (`evidence/controller-native.txt`). Dies ist kein Editor-Attach-Test.
- Originale Tape-JA- und Vinyl-Tracing-/Surrogat-Referenztests: **3 bestanden**. Diese Referenzen sind zusätzliche Modellvergleiche, keine zertifizierten Hardware-Orakel.
- Nicht im ausgelieferten Signalpfad verwendeter **Console-ADAA-Produktionskandidat: FAIL** in seinem CPU-/Abweichungsgate (`evidence/oracle-tests.txt`). Nicht aktiviert, nicht repariert/abgeschwächt, nicht als Release-DSP-PASS gezählt.
- ParamID-/Default-/Routing-/Channel- und XML/Geometrie-Contracts bestanden. Die vollständige Bedienmatrix ist `CONTROL-MATRIX-2026-10-14.md`; jede echte Bedien-/DAW-Lücke ist dort NOT VERIFIED.
- Wiederverwendbare Setup-Rezeptur vollständig ausgeführt; GNU-DSP-Build und beide Linux-VST3-Bundles hergestellt. Das installierte Dateisystem und laufende Prozesse sind getrennt von einem künftig veröffentlichten Snapshot.

## 4. DSP-Messungen und Klangbewahrung

- Eingeschwungene aktive synthetische Gesamtprogrammsequenz (48 kHz, alle Module gemäß bestehendem program_material-Fixture): Ausgangs-FULL und Reparatur liefern **bitidentische 48 000 double-Samples**; SHA-256 beider Outputs: `e743195038ae3eab6981b34a35e3b59776913d57cbf616b5aae1c0bcd85ff1a4`. Das gilt für diesen eingefrorenen Prüffall, nicht für alle denkbaren Presets/Audioprogramme.
- Bypass-Wet-Wiederaufnahme gegenüber kontinuierlicher Referenz: maxDiff **0** nach der 5-ms-Überblendung. Bypass-Dry-Fehler **0**.
- Gemessener letzter Impuls-Nachlauf oberhalb 1e−9: 15 261 Samples @44.1 kHz; 16 309 @48 kHz; 28 382 @96 kHz; 56 744 @192 kHz. Gemeldete 2-s-Grenze jeweils darüber. Rauschen benötigt separat unendlichen Tail.
- Gemeinsame feste Hostlatenz bleibt 28/29/36/50 Samples @44.1/48/96/192 kHz; vorhandene Impuls-/Quality-/Modul-/Phase- und Formatmatrizen bestehen.
- Weitere THD/Restfehler, Alias-/Level-/Speed-/Voice-/Stereo-/Noise-Messungen sind in den bestehenden ausführbaren Diagnosequellen spezifiziert. Keine neuen Hardware-Authentizitätsbehauptungen: die generischen Modelle und empirischen Kompensationsflächen bleiben als solche begrenzt.
- Zusätzlich vorhandener SDK-Drumloop: `public.sdk/samples/vst/again_auv3/Shared/drumLoop.wav`, SDK-Pin wie Setup, ursprünglicher SHA-256 `7e3fd3a093fcc4b94418664adac487781e402ed26e75fef9a5046476a0f38522`; 44.1-kHz PCM16 Stereo, 211681 Frames, auf Mono gemittelt und explizit linear nach 48 kHz interpoliert. Vier Faktoren 0.25/0.5/1/2 durch identisch konfigurierte FULL-/Audit-Renderer: je 230401 Samples bitidentisch und endlich; Peaks 0.173093/0.223118/0.307892/0.674730, RMS 0.0272783/0.0518597/0.100504/0.197726. `evidence/real-drum-reference.json` und Rendererquellen dokumentieren den cloudseitig reproduzierbaren Vergleich. SDK-Lizenz MIT; Fixture nicht in kommerzielle Pakete kopiert. Aufnahme-/Syntheseherkunft des Drumloops nicht separat bewiesen.
- **Vollständige reale Mix-/Bass-/Vocal-Fixtures und musikalische Level-Match-/A/B-Abnahme: NOT VERIFIED.** Das program_material-Fixture ist synthetisch und wird nicht als reale Aufnahme ausgegeben.

## 5. Performance / Echtzeit

`evidence/realtime-final.txt` enthält 32/64-bit, Mono/Stereo, vier Sample-Rates, Blockgrößen 1/7/64/127/256, komplette High-4x-Kette, 600 Messblöcke nach Warmup, Offline-Durchsatz und gepacete Callback-Prüfung. System/Compiler/Quota: `evidence/environment.txt`. Timerpaar-Overhead ist ausgewiesen. C++ new/new[]-Aufrufe im überwachten Callback: **0**; dies deckt nicht jede mögliche C-/Aligned-Allokation oder einen realen verbundenen Host ab.

**Realtime-Gate: FAIL**, keine Entwarnung aus Mittelwerten. Beispielsweise 192 kHz / Stereo / double / Block 64: p99 231.930 µs, max 592.172 µs, Deadline 333.333 µs, 1 Overrun. Der separate Thread-CPU-Maxwert war 273.806 µs; dies zeigt bei diesem Ausreißer Scheduling-Anteil, erklärt aber nicht sämtliche Ausreißer. Eine frühere Messung lief parallel zu Builds und wird nicht als maßgebliche ruhige Endmessung genutzt. Zusätzlicher ruhiger Lauf mit elf gleichzeitigen kontinuierlichen Rampen: **FAIL**, 0 überwachte C++-Allokationen. @48 kHz Block64: p99 668.376 µs / max 1813.18 µs, 1 Overrun; @192 kHz: p99 368.725 µs / max 608.315 µs, 10 Overruns gegenüber 333.333 µs Deadline. Thread-CPU-Max @192 kHz 609.924 µs. Evidenz `evidence/realtime-dense.txt`; die Windows-Vorlage führt dieses Gate separat aus. Dies ist eine Cloud-Messung, keine abschließende Ursachenzuordnung oder Windows-Entwarnung. Windows-/Studio-One-Realtime, typische Projektlast und höchste Instanzzahlen: **NOT VERIFIED**. Die Vorlage lässt das Timing-Gate bestehen und sammelt seine Fehlerausgaben.

## 6. VST3 / GUI / 125A QA

- Kompiliertes **Linux Channel**-VST3: **47/47 Steinberg Validator PASS**, nach CLIP-Textkorrektur ohne die vorherige Off/On-Konvertierungslücke; `evidence/validator-channel-final.txt`.
- Mix FX-Kategorie: Steinberg führt **0 Tests** aus. Das ist **kein Validator PASS**; Mix FX API wird separat in Processor-Tests geprüft und benötigt Studio One.
- Vollständige native Pixelbilder wurden geprüft: 1440×800 bei 100 % sowie 2160×1200 bei 150 % externem Content-Scale. Das UI-interne Zoomlabel bleibt dabei korrekt 100 %; dies ist kein Beweis der fünf internen Zoomstufen. Bilder in `evidence/editor-100.png`, `evidence/editor-150.png`. Master-SVG ist byteidentisch zum aktuellen Branding-Repository (SHA-256 `54984ddaea38043e1449b2e5a8e80351ed73e66d922533814701b98b8fc3bc39`).
- Linux-Attach-Harness benötigt Runloop im Factory-Host-Kontext und XEmbed-Handschlag; diese Harness-Fehler wurden korrigiert. Bei wiederholtem Öffnen abortiert Cairo/VSTGUI reproduzierbar um Zyklus 4 mit `_get_screen_index: Assertion !reached`. Stack in `evidence/editor-reopen-backtrace.txt`. **Linux Editor Lifecycle FAIL / genaue Verantwortungszuordnung nicht vollständig bewiesen**. Keine SDK-Quellen geändert, kein Linux-Release vorbereitet.
- Vollständiger 125A Tester auf Linux nicht ausführbar: gepinnter Quellstand benötigt `windows.h` in report_paths.h trotz vorhandenem Linux-Modullader. Keine Tester-Implementierung wurde verändert, um einen Ersatz-PASS zu erzeugen.
- Windows-Audit: erster tatsächlicher Lauf [38651260777](https://github.com/challanger2000/125A-MixEngine-Final/actions/runs/38651260777), SHA `40aa2fd9bf7af2918d0ce89e2fad0c164237a773`: **SETUP FAIL**, private Tester-Checkout bekommt repository not found; native Builds/Tests wurden übersprungen. Repo-lokaler Actions-Token reicht für dieses zweite private Repository nicht. Folgepipeline verschiebt diese Zugriffsschranke hinter die unabhängigen Funktionsprüfungen und erwartet optional das Repository-Secret `AUDIT_QA_READ_TOKEN` mit Contents-read für `125A-Plugin-Tester` (nur in GitHub Settings eintragen, keine Werte im Chat). Vollständige Windows-QA weiterhin **NOT VERIFIED**. `.github/workflows/audit-v3.yml` ist eine neue gebündelte QA-Pipeline für beide Varianten, 58 Funktions-/Controller-/Realtime-Ziele plus dichter Automationslauf sowie gepinnten offiziellen 125A Tester und nativen Editor-Probe. Ein Push-Trigger ausschließlich auf diesem Audit-Branch startet den gebündelten Lauf trotz blockierter Dispatch-API; zusätzlich workflow_dispatch. Kein Gumroad-/Demo-Paket, keine Release-Erstellung. Targetnamen gegen die tatsächlich gepinnten CMake-Dateien geprüft; Windows/Powershell-Ausführung steht aus.

## 7. Verbleibende Prüfungen / technische Blockaden

1. GitHub Actions benötigt Zugriff auf das private gepinnte `125A-Plugin-Tester`-Repository; falls kein geeignetes Secret existiert, `AUDIT_QA_READ_TOKEN` ausschließlich in den Repository-Settings hinterlegen, Contents-read für den Tester. Dies ist eine andere Zugriffsschranke als der Cloud-API-Forbidden. Einen Windows-Audit-Lauf auf dem Audit-Branch starten und dessen exakten head_sha, Jobs, Logs und neuen Tester-Reports auswerten. GitHub-API-Anfragen liefern hier Forbidden, trotz vorhandenem GH_TOKEN und funktionierender Git-Authentifizierung. Keinen weiteren Token ohne spezifische Diagnose anfordern. `api.github.com` wurde additiv in den Konfigurationsentwurf aufgenommen; dieser Entwurf ist noch kein veröffentlichter/applizierter Runtime-Zustand.
2. Alle tatsächlichen Windows-Bedienelemente inklusive Drag, Wheel, Ctrl-Default, beide Toggle-Richtungen, Selektor-Wiederwahl, Zoom 75/90/100/125/150, Automation-Feedback und Editor-Reopen prüfen. Existenz und DSP-Queue-Tests ersetzen dies nicht.
3. Verlässliche Windows-/Studio-One-Deadline- und hohe Instanzlast-Messung; cloudseitiges FAIL nicht verschweigen.
4. Feste echte Mix-/Drum-/Bass-/Vocal-Fixtures mit gesicherter Herkunft, Input-Level-Sweep und level-gematchtem musikalischem A/B.
5. Echte Host-Projekt-/Preset-Recall, simultane Audio-/Control-Kontexte und Data Exchange mit aktivem Editor; noch kein Thread-Stress-/Race-Detector-Nachweis.
6. Optionaler Linux-Lifecycle-Abbruch weiter isolieren, falls Linux-Unterstützung künftig Produktziel wird. Das ist von der vorgesehenen Windows-Freigabe getrennt.

## 8. Quellstand

Repository `challanger2000/125A-MixEngine-Final`; Branch `audit/v3.0.0-release-2026-10-14`. Reparatur-/Evidenz-Commit: `88d8014f6e149927de3fa8da7cbcbc4628eae8dd`; ein nachfolgender QA-Trigger-Commit enthält ausschließlich den branchbegrenzten Windows-Start und diese Dokumentation. Die exakte finale Übergabe-SHA wird beim Commit/Push in der Chat-Übergabe und Git-Historie festgehalten; dieser Bericht ist Bestandteil dieses Commits. `main`, `v3.0.0-development`, `Demo` und die bestehenden Release-Pipelines wurden nicht überschrieben. Ein ergänzender Guardrail steht auf einem eigenen Engineering-Audit-Branch.

## 9. Studio-One-Abnahme durch den Nutzer

- Beide Varianten getrennt installieren/rescannen; Mix FX im tatsächlichen Mix-FX-Slot, Channel als Insert; gespeicherte V3-Projekte und Presets laden.
- Jeden Matrix-Eintrag tatsächlich bedienen; Ctrl-Defaults, Wheel/Drag, Auswahl/Toggle ON/OFF, Anzeige und Automation prüfen; alle fünf internen Zoomstufen, Größenverhandlung und mindestens 20 Reopen-Zyklen.
- Bypass beim laufenden komplexen Material hin-/zurückschalten; kein kalter Wet-Neustart, keine Pegelsprünge oder versehentliche Unterbrechung der Rausch-/Zeitmodelle.
- Adjacent-channel Crosstalk und V3-Bussummeninteraktion mit mehreren Mono/Stereo-Quellen, Kanalumordnung, Stummschaltung und wechselnden aktiven Quellen prüfen.
- 44.1/48/96/192 kHz, kleine/große Blöcke, Automation, Transport Stop/Start, offline Bounce mit vollständigem Nachlauf; Silence/Noise/Clip-Anzeigen und gestoppten Transport prüfen.
- Projekt speichern, Editor schließen, DAW neu öffnen: Parameter, Sound, Meterquelle und Referenz konsistent; gewohnte V3-Grundfärbung/Level Match musikalisch freigeben.

## 10. Hinweise an ChatGPT für die spätere Release-Erstellung

Erst fehlende technische Gates nachholen und bewerten; diesen Stand nicht blind AUDIT PASSED oder FINAL nennen. Keine funktionalen Änderungen zur Erzeugung eines grünen Berichts ohne neue Regression/Begründung. Anschließend nur vom geprüften Audit-Commit weiterarbeiten. Finales Windows-VST3, Versionsnummer, Gumroad-FULL, Demo-Paket und Handbücher/Lizenz-/Release-Dateien bleiben beim nachgelagerten ChatGPT-Workflow. Demo nicht vom falsch etikettierten älteren DSP-Zweig übernehmen; neue Lizenzintegration wäre eine getrennt zu verifizierende Produktänderung. Finale Freigabe ausschließlich nach Nutzer-Studio-One-Abnahme.
