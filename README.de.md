# aud_dsp_effects

[English](README.md) | Deutsch

[![CI](https://github.com/audaudio/aud_dsp_effects/actions/workflows/quick_check.yaml/badge.svg)](https://github.com/audaudio/aud_dsp_effects/actions/workflows/quick_check.yaml)

Effekte der Audanika Audio Engine: Hall, Delays, Oszillatoren, Filter,
Dynamik, Modulation, Verzerrung. Heute liefert das Paket Freeverb, einen
Plattenhall, ein Stereo- und ein tempo-synchrones Delay, einen Limiter mit
Look-ahead für die Summe eines Graphen und ein Tremolo. Die Nodes laufen
auf der Engine von `aud_audio` unter iOS und Android, rampen ihre
Parameter und allokieren nichts auf dem Audio-Thread.

Teil der Audanika Audio Engine; geplant in [aud_audio_pm](https://github.com/audaudio/aud_audio_pm).

## Motivation

- Hall, Delay und ein Master-Limiter als Nodes der Engine (Ticket 28,
  Schritt S10a), für die Master-Kette einer App: Delay, Hall, Limiter
- Nur Code mit klarer Herkunft: Freeverb (Public Domain), StereoDelay von
  DunneAudioKit (MIT) und eigener Code nach der Literatur; kein Faust,
  kein Soundpipe, kein LGPL (dsp-001, license-002)
- Ein Limiter, dessen Ausgang die Obergrenze nie überschreitet, auch 24 dB
  über Vollaussteuerung, mit dem Look-ahead als gemeldeter Latenz
- Nichts allokiert, sperrt oder blockiert auf dem Audio-Thread; die
  nativen Tests prüfen jeden Node unter ASan/UBSan, dem
  RealtimeSanitizer und TSan
- Die Renderzeit jedes Nodes, gemessen auf dem Gerät, das ihn spielt
- Eine App ohne die Effekte liefert ihre Bibliothek nicht aus
  (interop-001)

## Installation

```yaml
dependencies:
  aud_dsp_effects:
    git:
      url: git@github.com:audaudio/aud_dsp_effects.git
      tag_pattern: "{{version}}"
    version: ^0.2.0
```

Eine App hängt von `aud_audio` und den DSP-Paketen ab, die sie nutzt, und
registriert diese bei ihrer Engine (family-001). Der Build-Hook übersetzt
die Nodes mit der C++-Toolchain des Ziels gegen den ABI-Header von
`aud_audio_core`; das Paket braucht kein Flutter. Die nativen Tests
brauchen Node.js und `clang++`; ihr RealtimeSanitizer-Build braucht ein
clang, das ihn hat, etwa LLVM aus Homebrew unter macOS
(`brew install llvm`), und entfällt ohne eines.

```bash
dart test                       # Dart tests, goldens, native tests
node scripts/test-native.js     # the native tests alone
node scripts/check-notices.js   # the third-party notices
UPDATE_GOLDENS=1 dart test test/aud_effects_golden_test.dart
```

## Nutzung

Die Master-Kette einer App auf der Engine von `aud_audio`
(`aud_audio_ffi.dart` auf nativen Plattformen): die Summe der Instrumente
läuft durch Delay, Platte und Limiter zum Ausgang.

```dart
import 'package:aud_audio/aud_audio_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';

void main() {
  final engine = AudEngine(
    packages: [
      AudNativeNodePackage(AudDspEffects.name, AudDspEffects.register),
    ],
  );
  engine.prepare();
  final graph = engine.graph;
  final delay = graph.createNode(AudStereoDelay.typeId);
  final plate = graph.createNode(AudPlateReverb.typeId);
  final limiter = graph.createNode(AudLimiter.typeId);
  graph.setParam(delay, AudStereoDelay.time, 0.4);
  graph.setParam(delay, AudStereoDelay.feedback, 0.15);
  graph.setParam(delay, AudStereoDelay.mix, 0.06);
  graph.setParam(delay, AudStereoDelay.pingPong, 1);
  graph.setParam(plate, AudPlateReverb.mix, 0.15);
  graph.transaction(
    (tx) => tx
      ..connect(delay, plate)
      ..connect(plate, limiter)
      ..connect(limiter, graph.io),
  );
  engine.start();
  // Connect the instruments to `delay`; their connections sum.

  // A preset with more reverb, ramped over 100 ms.
  graph.setParam(plate, AudPlateReverb.mix, 0.3, rampFrames: 4800);
  graph.setParam(plate, AudPlateReverb.decay, 0.7, rampFrames: 4800);
  print(graph.latencyOf(limiter)); // 72 frames at 48 kHz: the look-ahead
}
```

Die Parameter, wie die Nodes sie deklarieren, ohne Engine und im Web
(`aud_dsp_effects.dart` importiert kein `dart:ffi`):

```dart
import 'package:aud_dsp_effects/aud_dsp_effects.dart';

void main() {
  for (final param in AudPlateReverb.paramDescriptors) {
    print('${param.id}: ${param.min} to ${param.max}, ${param.defaultValue}');
  }
  print(AudTempoDelay.divisions); // [1/32, 1/16T, 1/32D, 1/16, ...]
  print(AudLimiter.lookaheadFrames(44100)); // 66
}
```

Die Renderzeit eines Nodes, gemessen auf dem aufrufenden Thread - in
`dart test` oder in einem Integrationstest auf einem Telefon:

```dart
import 'package:aud_dsp_effects/aud_dsp_effects.dart';
import 'package:aud_dsp_effects/testing.dart';

void main() {
  final result = AudEffectsBench.measure(
    AudPlateReverb.typeId,
    params: {AudPlateReverb.decay: 0.7},
  );
  print(result); // aud.effects.plate: mean 3.5 µs, p99 4.5 µs, ...
  print(result.toJson());
}
```

### Die Beispiel-App

`example/` spielt einen dichten Akkord - acht Sägezähne über drei
Oktaven - durch das Stereo-Delay, die Platte oder Freeverb und den
Limiter auf der Engine von `aud_audio`, auf iOS und Android: ein Regler
je Parameter, ein Drive in den Limiter bis +24 dB, die Renderzeit, die
Xruns, die Ausgangsspitze und die Latenz des Limiters, und ein Bericht zum
Kopieren.

```bash
cd example
flutter run -d <device>
flutter test integration_test/effects_test.dart -d <device> \
  --dart-define=AUD_MEASURE_SECONDS=10
# The same on an iPhone over Wi-Fi
flutter drive --driver=test_driver/integration_test.dart \
  --target=integration_test/effects_test.dart -d <device> --publish-port \
  --dart-define=AUD_MEASURE_SECONDS=10
```

Der Integrationstest gibt zwei Zeilen JSON aus. `EFFECTS_BENCH` enthält
die Bench jedes Nodes bei der Abtastrate und dem Block des Geräts und bei
48 kHz, 256 Frames. `EFFECTS_CHAIN` enthält den Akkord, live gespielt für
`AUD_MEASURE_SECONDS` (Vorgabe 10), erst direkt zum Ausgang, dann durch
Delay, Platte und Limiter, 12 dB über Vollaussteuerung gefahren:
Callbacks, Xruns, verspätete Callbacks, die Renderzeit mit und ohne die
Effekte und die Ausgangsspitze, die an der Obergrenze bleibt; dazu die
Latenz des Limiters, die ein offline auf dem Gerät gerechneter Impuls
bestätigt.

## Einschränkungen

- Von den Effekten seiner Beschreibung liefert das Paket heute Hall,
  Delays, einen Limiter und ein Tremolo; Oszillatoren, Filter, Verzerrung
  und weitere Modulation sind in `aud_audio_pm` geplant
- Kein Faust, kein Soundpipe, kein LGPL: ein Node wird nur mit klarer
  Herkunft übernommen (dsp-001, license-002)
- Der Limiter verzögert das Signal um seinen Look-ahead, 1,5 ms (72 Frames
  bei 48 kHz), und meldet ihn als seine Latenz
- Das Tempo-Delay spielt höchstens 4 s: Teilungen, die beim Tempo des
  Transports länger sind, verlassen den Schlag
- Das Stereo-Delay klingt ab Feedback 0.999 nie mehr ab

## Dokumentation

- [Der Plan von Ticket 28](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/tickets/2026-10-13-28-build-reverb-delay-and-a-master-limiter.md)
  mit den Nodes, ihren Voreinstellungen und den Messungen
- Die Entscheidungen hinter dem Paket:
  [dsp-001](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/dsp-001.md)
  (Quellen mit klarer Herkunft),
  [license-002](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/license-002.md)
  (MIT, kein LGPL),
  [abi-001](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/abi-001.md)
  (die C-ABI),
  [build-001](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/build-001.md)
  (fremdes C++, vom Hook übersetzt)
- Die C-API: [`src/aud_dsp_effects.h`](src/aud_dsp_effects.h)
- Der übernommene Code, seine Lizenzen und die Prüfung:
  [`src/third_party/NOTICES.md`](src/third_party/NOTICES.md)
- Die Guides in [`doc/guides`](doc/guides)

## Funktionsweise

Jeder Node rechnet seinen Eingang an Ort und Stelle, nimmt jeden Block bis
zum vorbereiteten Maximum und rampt seine Parameter über die
Parameter-Events der Engine (`AudNodeBase` aus `aud_audio_core`). Puffer
entstehen in `prepare`; `process`, `reset` und `set_param` allokieren und
sperren nicht. Hall und Delays nehmen einen Mono- oder Stereo-Eingang -
ein Mono-Eingang speist beide Seiten - und geben Stereo aus; `mix` blendet
linear von trocken (0) nach nass (1). Rückkopplungen setzen Werte unter
1e-15 auf null, damit lange Ausklänge nie an Denormals langsam werden.

| Node | Parameter (Bereich, Vorgabe) | Latenz, Ausklang | Quelle |
| --- | --- | --- | --- |
| `aud.effects.freeverb` | `room_size` (0..1, 0.5), `damping` (0..1, 0.5), `width` (0..1, 1), `mix` (0..1, 0.25) | 0; bis der längste Kamm um 60 dB abgeklungen ist, höchstens 30 s | Freeverb von Jezar at Dreampoint, Public Domain; Tunings von 44,1 kHz skaliert |
| `aud.effects.plate` | `pre_delay` (s, 0..0.25, 0.02), `bandwidth` (0..1, 0.9995), `decay` (0..0.99, 0.5), `damping` (0..1, 0.0005), `mix` (0..1, 0.25) | 0; bis der Tank um 60 dB abgeklungen ist, höchstens 30 s | Eigener Code nach J. Dattorro, "Effect Design, Part 1", JAES 45(9), 1997; Verzögerungen von 29761 Hz skaliert |
| `aud.effects.stereo_delay` | `time` (s, 0.001..2, 0.4), `feedback` (0..1, 0.3), `mix` (0..1, 0.25), `ping_pong` (0/1, 0) | 0; bis die Echos um 60 dB abgeklungen sind, endlos ab Feedback 0.999 | StereoDelay von DunneAudioKit, MIT, Copyright (c) 2021 AudioKit; Mix nicht mehr vertauscht |
| `aud.effects.tempo_delay` | `division` (0..15, 8 = 1/8D), `feedback` (0..1, 0.3), `mix` (0..1, 0.25), `ping_pong` (0/1, 0) | 0; wie beim Stereo-Delay | Die Leitungen des Stereo-Delays; Zeit = Schläge(division) · 60 / Tempo des Transports (120 BPM ohne), bis 4 s; längere Teilungen spielen 4 s und verlassen den Schlag |
| `aud.effects.limiter` | `ceiling` (dBFS, -24..0, -1), `release` (s, 0.001..2, 0.1), `gain` (dB, -24..24, 0) | 1,5 ms (72 Frames bei 48 kHz) für beides | Eigener Code nach Giannoulis, Massberg, Reiss, JAES 60(6), 2012, und Zölzer, DAFX Kap. 4 |
| `aud.effects.tremolo` | `rate` (Hz, 0.1..20, 5), `depth` (0..1, 0.5) | 0; 0 | Eigener Code |

Die Delays gleiten mit einer Zeitkonstante von 50 ms zu einer neuen Zeit,
wie ein Band, statt zu knacken. Das Tempo-Delay liest das Tempo des
Segments am Anfang jedes Blocks; seine Teilungen sind nach Länge sortiert:
1/32, 1/16T, 1/32D, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D,
1/2, 1/1T, 1/2D, 1/1.

Der Limiter verstärkt den Eingang, bestimmt je Frame die Verstärkung, die
unter der Obergrenze bleibt - über alle Kanäle gekoppelt -, hält ihr
Minimum über den Look-ahead von L Frames, lässt einen Release-Folger sie
höchstens mit dem Release anheben, glättet sie mit einem Boxfilter aus
L + 1 Frames und wendet sie auf das um L Frames verzögerte Signal an,
sodass jede Spitze auf die voll abgesenkte Verstärkung trifft; eine
letzte Begrenzung auf die Obergrenze fängt Rundungen ab. Werte, die nicht
endlich sind - keine Zahl oder unendlich, auch nach der Verstärkung -,
zählen als Stille.

Die Bench (`testing.dart`) registriert das Paket bei einem eigenen Host,
rechnet einen Node über seine vtable und misst jeden Process-Aufruf mit
einem dichten Akkord bei -6 dBFS, der alle 4 Sekunden in Stille ausklingt.
Auf einem Apple M5 bei 48 kHz und 256 Frames je Block liegt das Mittel je
Block bei etwa 4 µs für Freeverb, 3,5 µs für die Platte, 2 µs je Delay,
1 µs für den Limiter und 0,6 µs für das Tremolo - weit unter 0,1 % des
Blocks.

### Die Dart-API

- `aud_dsp_effects.dart` ist plattformneutral und übersetzt fürs Web: je
  Node eine Klasse - `AudFreeverb`, `AudPlateReverb`, `AudStereoDelay`,
  `AudTempoDelay`, `AudLimiter`, `AudTremolo` - mit ihrer `typeId`, ihrer
  `version`, den Parameter-IDs, `params` in der Reihenfolge ihrer Indizes
  und `paramDescriptors` mit Bereichen und Vorgaben. `AudTempoDelay`
  ergänzt `divisions`, `divisionBeats` und `maxSeconds`, `AudLimiter`
  ergänzt `lookaheadSeconds` und `lookaheadFrames(sampleRate)`.
  `audDspEffectsVersion` ist die Version des Pakets.
- `aud_dsp_effects_ffi.dart` ergänzt `AudDspEffects`: `name` und
  `register` - die Register-Funktion - für die Paketliste einer Engine;
  `registerWith` registriert beim Host-API eines Graphen und wirft eine
  `AudDspEffectsException` mit dem Ergebnis-`code` und der `operation`,
  wenn die Engine ablehnt.
- `register`, `registerWith` und `AudEffectsBench.measure` sind mit
  `@RecordUse` aufgezeichnet: in einem Profile- oder Release-Build behält
  der Link-Hook die native Bibliothek nur, wenn die App eines davon nutzt.
- `testing.dart` ergänzt `AudEffectsBench.measure`, das ein
  `AudEffectsBenchResult` liefert: das Mittel, das p99 und das Maximum in
  Nanosekunden, ihre Anteile am Block und `toJson()`.

## Status

Vor 1.0 — die API kann sich zwischen Minor-Versionen ändern. Das Paket liefert die Nodes von Ticket 28
(Schritt S10a); die übrigen Effekte seiner Beschreibung sind in
`aud_audio_pm` geplant.

## Mitwirken

Gearbeitet wird Ticket für Ticket mit dem `gg`-Workflow; siehe den
[Develop Guide](doc/guides/develop-guide.md) und den
[Review Guide](doc/guides/for-ai/ai-review-guide.md). Jeder Commit besteht
`dart analyze`, `dart format`, `dart test` mit 100 % Abdeckung von `lib/`,
die nativen Tests und die Prüfung der Hinweise.

## Lizenz

MIT — siehe [LICENSE](LICENSE). Der übernommene Code unter
`src/third_party` behält seine eigenen Lizenzen;
[`src/third_party/NOTICES.md`](src/third_party/NOTICES.md) nennt sie und
die Prüfung der Herkunft jedes Nodes.
