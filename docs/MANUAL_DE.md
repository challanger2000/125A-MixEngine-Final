# 125A MixEngine - Bedienungsanleitung

Version 1.0.0 - Windows x64 VST3

## 1. Überblick

125A MixEngine ist ein modularer Coloration- und Summing-Prozessor mit zwei Varianten: **125A MixEngine** als PreSonus Studio One Mix FX und **125A MixEngine Channel** als normaler VST3-Insert. Beide Varianten verwenden denselben zentralen DSP-Kern. Nur die Mix-FX-Variante kann auf benachbarte DAW-Kanäle zugreifen und deshalb echtes Kanal-zu-Kanal-Crosstalk erzeugen.

Signalfluss: **INPUT -> CONSOLE -> TUBE -> TAPE -> GLUE -> VINYL -> STEREO -> OUTPUT**.

0 VU ist ein Referenzpunkt, kein Pegel, den jedes Musiksignal permanent treffen muss. Bei dynamischem Material kann die VU-Anzeige deutlich niedriger sein als der DAW-Peak.

## 2. Installation und Varianten

- **125A MixEngine.vst3**: PreSonus Studio One Mix FX. In den Mix-FX-Slot eines Bus/Main-Bereichs laden.
- **125A MixEngine Channel.vst3**: normaler VST3-Insert für Einzelkanäle, Busse und andere VST3-Hosts.
- Installation unter Windows: VST3-Bundle nach `C:\Program Files\Common Files\VST3` kopieren und Host neu scannen.
- Beide Varianten besitzen getrennte Plugin-IDs und können parallel installiert werden.

## 3. Metering, Reference Level und Gain Staging

**Reference Level** legt fest, welcher digitale Pegel 0 VU entspricht: -18, -14 oder -10 dBFS. Standard ist -14 dBFS.

Das VU-Meter arbeitet energiebezogen mit etwa 300 ms Einschwingverhalten. Es ist kein Peak-Meter. Die CLIP-Anzeige reagiert bei Peak >= 0 dBFS und hält etwa 0.75 s.

**VU Source Input** misst nach Input Gain. **VU Source Output** misst nach dem gesamten DSP inklusive Output Gain.

**Level Match** ist eine feste, parameterabhängige Kompensation. Es ist kein adaptiver Loudness-Normalizer.

## 4. Vollständige Bedienelemente

| Bedienelement | Typ | Bereich | Default | Funktion |
|---|---|---|---|---|
| Bypass | Schalter | Active / Bypass | Active | Umgeht Klangbearbeitung, behält aber die feste Host-Latenz bei. |
| Input Gain | Regler | -12.0 bis +12.0 dB | 0.0 dB | Pegel vor allen Klangmodulen. |
| Reference Level | Wahlschalter | -18 / -14 / -10 dBFS | -14 dBFS | Definiert 0 VU und die interne Kalibrierung. |
| Level Match | Schalter | Off / On | On | Feste Pegelkompensation der Klangmodule. |
| Console On | Schalter | Off / On | On | Aktiviert die Console-Sektion. |
| Console Mode | Wahlschalter | Clean / Classic / Vintage / Modern | Classic | Vier generische Console-Kennlinien. |
| Console Drive | Regler | 0 bis 100 % | 25 % | 0 % ist im nichtlinearen Core neutral; höherer Drive erhöht Sättigung, Peak-Rundung und Dichte. |
| Crosstalk | Regler, nur Mix FX | 0 bis 100 % | 10 % | Echtes Übersprechen zu direkten Nachbarkanälen; maximal nominal ca. 1.8 % (-34.9 dB) pro Nachbar. |
| Console Noise | Regler | 0 bis 100 % | 0 % | Separates gefärbtes Console-Grundrauschen. |
| Tube On | Schalter | Off / On | Off | Aktiviert Tube. |
| Tube Voice | Wahlschalter | Soft / Balanced / Hot | Balanced | Wählt zunehmend stärkere Drive-/Bias-/Asymmetrie-Charakteristik. |
| Tube Amount | Regler | 0 bis 100 % | 20 % | Stärke der Tube-Sättigung; 0 % ist neutral. |
| Tape On | Schalter | Off / On | Off | Aktiviert Studio-Tape. |
| Tape Amount | Regler | 0 bis 100 % | 20 % | Tape-Sättigung, weiche Verdichtung und Wet-Anteil. |
| Tape Speed | Wahlschalter | 7.5 / 15 / 30 ips | 15 ips | Bestimmt Bandbreite, Low-Bump und Transportcharakter. |
| Tape Stability | Regler | 0 bis 100 % | 90 % | 100 % = stabil; niedrigere Werte erhöhen Wow/Flutter. |
| Tape Hiss | Regler | 0 bis 100 % | 0 % | Separates bandtypisches Hiss. |
| Glue On | Schalter | Off / On | Off | Aktiviert Glue. |
| Glue Amount | Regler | 0 bis 100 % | 15 % | Erhöht Kompressionswirkung und Blend. |
| Glue Response | Regler | 0 bis 100 % | 50 % | Niedrig = langsamer/weicher; hoch = schneller/straffer. |
| Vinyl On | Schalter | Off / On | Off | Aktiviert Vinyl. |
| Vinyl Color | Regler | 0 bis 100 % | 25 % | Grundcharakter: weichere Höhen, etwas Body und Sättigung. |
| Vinyl Wear | Regler | 0 bis 100 % | 0 % | Abnutzung: mattere Höhen, zusätzliche Färbung und Sättigung. |
| Vinyl Surface | Regler | 0 bis 100 % | 0 % | Oberflächenrauschen plus sporadische Klicks/Pops. |
| Depth | Regler | -100 bis +100 % | 0 % | Bearbeitet oberen Side-Bereich um ca. 2 kHz und darüber; Endpunkte ca. +/-4 dB. |
| Width | Regler | 0 bis 200 % | 100 % | M/S-Breite; 100 % ist neutral. |
| Low Mono | Regler | 0 bis 100 % | 0 % | Entfernt stufenlos tiefen Side-Anteil mit festem 120-Hz-Übergang. |
| Output Gain | Regler | -12.0 bis +12.0 dB | 0.0 dB | Finaler Pegel nach allen Modulen. |
| VU Source | Wahlschalter | Input / Output | Output | Wählt Meterposition. |
| Quality | Wahlschalter | Eco 1x / Normal 2x / High 4x | Normal 2x | Oversampling-Faktor relevanter nichtlinearer Stufen. |
| UI Scale | Taster | 75 / 100 / 125 / 150 % | 100 % | Jeder Klick schaltet zyklisch zur nächsten GUI-Größe. |
| VU L/R | Anzeige | -20 bis +3 VU | - | Energiebezogene VU-Anzeige. |
| CLIP L/R | Anzeige | aus / an | - | Peak >= 0 dBFS, Hold ca. 0.75 s. |

## 5. Console im Detail

**Drive 0 %** ist im nichtlinearen Console-Core ein echter Neutralpunkt. Die aktive Console kann trotzdem feste Eigenschaften wie Kanal-Toleranzen, DC-Entkopplung, optionales Noise und - in Mix FX - Crosstalk enthalten. Die eigentliche modusspezifische Sättigung steigt erst mit Drive.

- **Clean**: zurückhaltendste Kennlinie.
- **Classic**: mehr Dichte, leichte Low-End-Kopplung und kleine hochfrequente/asymmetrische Komponente.
- **Vintage**: stärkste Low-/Asymmetrie-Färbung und höchste Sättigungsneigung.
- **Modern**: stärker hochfrequenzbezogene Formung, vergleichsweise straff.

Crosstalk koppelt nur **direkte Nachbarn**. Kanal 1 koppelt zu Kanal 2, nicht direkt zu Kanal 3. Ein mittlerer Kanal kann zu beiden direkten Nachbarn koppeln. Die Kopplung ist lane-preserving: links bleibt links, rechts bleibt rechts. Der Standard-Channel hat diesen Regler bewusst nicht.

## 6. Tube

Soft ist die mildeste, Balanced die mittlere und Hot die stärkste Voice. Amount 0 % ist neutral. Mit steigendem Amount nehmen Drive, harmonische Dichte und Asymmetrie zu.

## 7. Tape

Amount steuert Sättigung und weiche Verdichtung. **7.5 ips** rollt die Höhen früher ab und hat den stärksten Low-Bump, **15 ips** ist ausgewogen, **30 ips** ist offener und sauberer. Stability 100 % bedeutet stabilen Transport; niedrigere Werte erhöhen Wow/Flutter. Hiss ist separat und kann bei 0 bleiben.

## 8. Glue

Glue ist kein voll parametrisierter Bus-Kompressor. Amount erhöht die Kompressionswirkung. Response verschiebt interne Attack-/Release-Zeiten von langsamer/weicher zu schneller/straffer. Der Stereo-Detektor ist verlinkt.

## 9. Vinyl

**Color** setzt den Grundcharakter. **Wear** simuliert Abnutzung und kann das Signal dunkler und gesättigter machen, ohne zwingend Knistern hinzuzufügen. **Surface** ist die eigentliche Oberflächenebene mit Rauschen und Klick/Pop-Ereignissen; Wear beeinflusst dann auch deren Charakter.

## 10. Stereo

Width ist die klassische M/S-Breitensteuerung. 100 % ist neutral.

Depth ist bewusst kein zweiter Width-Regler. Es wirkt nur auf den oberen Side-Bereich um ca. 2 kHz und darüber. +100 % dämpft diesen Bereich um etwa 4 dB, -100 % hebt ihn um etwa 4 dB an.

Low Mono verwendet einen festen 120-Hz-Übergang und entfernt stufenlos den tiefen Side-Anteil. 100 % ist die volle Low-Mono-Wirkung, aber keine harte Brickwall-Trennung.

## 11. Quality, Latenz und Automation

Eco = 1x, Normal = 2x, High = 4x Oversampling in relevanten nichtlinearen Stufen.

Das Plugin meldet unabhängig vom Quality-Modus eine feste Host-Latenz von **21 Samples**. Interne kürzere Pfade werden auf dieses Ziel ausgerichtet; auch Bypass behält die Latenz.

Parameter sind host-automatisierbar. Die Verarbeitung verwendet den jeweils letzten Parameterwert des aktuellen Audio-Blocks; es wird keine sample-genaue Interpolation behauptet.

## 12. Technische Daten

- Format: VST3, Windows x64
- Editionen: PreSonus Studio One Mix FX + Standard VST3 Channel
- Audio: Mono/Stereo, 32-bit und 64-bit Sample Processing
- Feste Host-Latenz: 21 Samples
- Oversampling: 1x / 2x / 4x
- 0 VU: -18 / -14 / -10 dBFS
- VU Response: ca. 300 ms
- Clip Hold: ca. 0.75 s
- Low Mono: 120 Hz fixed
- Depth: oberer Side-Split um ca. 2 kHz, Endpunkte ca. +/-4 dB
- Mix FX Crosstalk: direkte Nachbarn, nominal max. Koeffizient 0.018

## 13. Hinweis zum Mastering

125A MixEngine ist ein Klangprozessor, kein Limiter und kein vollständiges Mastering-System. Finale Lautheit, True-Peak-Limitierung und Plattform-Targets werden mit separaten Mastering-Werkzeugen gesetzt.

Copyright 2026 125A Audio Software. Alle Rechte vorbehalten.
