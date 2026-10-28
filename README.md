# aud_dsp_effects

English | [Deutsch](README.de.md)

[![CI](https://github.com/audaudio/aud_dsp_effects/actions/workflows/quick_check.yaml/badge.svg)](https://github.com/audaudio/aud_dsp_effects/actions/workflows/quick_check.yaml)

Effects of the Audanika Audio Engine: reverbs, delays, oscillators,
filters, dynamics, modulation, distortion. Today the package ships
Freeverb, a plate reverb, a stereo and a tempo-synced delay, a look-ahead
limiter for the sum of a graph and a tremolo. The nodes run on the engine
of `aud_audio` on iOS and Android, ramp their parameters and allocate
nothing on the audio thread.

Part of the Audanika Audio Engine; planned in [aud_audio_pm](https://github.com/audaudio/aud_audio_pm).

## Motivation

- Reverb, delay and a master limiter as nodes of the engine (ticket 28,
  step S10a), for a master chain of an app: delay, reverb, limiter
- Code only with clear provenance: Freeverb (public domain), DunneAudioKit's
  StereoDelay (MIT) and Audanika's own code from the literature; no Faust,
  no Soundpipe, no LGPL (dsp-001, license-002)
- A limiter whose output never exceeds its ceiling, also 24 dB over full
  scale, with its look-ahead reported as the latency
- Nothing allocates, locks or blocks on the audio thread; the native tests
  run every node under ASan/UBSan, the RealtimeSanitizer and TSan
- The render time of each node measured on the device that plays it
- An app that does not use the effects ships without their library
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

An app depends on `aud_audio` and on the DSP packages it uses, and
registers them with its engine (family-001). The build hook compiles the
nodes with the C++ toolchain of the target against the ABI header of
`aud_audio_core`; the package needs no Flutter. The native tests need
Node.js and `clang++`; their RealtimeSanitizer build needs a clang that has
it, e.g. LLVM from Homebrew on macOS (`brew install llvm`), and is skipped
without one.

```bash
dart test                       # Dart tests, goldens, native tests
node scripts/test-native.js     # the native tests alone
node scripts/check-notices.js   # the third-party notices
UPDATE_GOLDENS=1 dart test test/aud_effects_golden_test.dart
```

## Usage

The master chain of an app on the engine of `aud_audio`
(`aud_audio_ffi.dart` on native platforms): the sum of the instruments
enters the delay, the plate and the limiter, then the output.

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

The parameters as the nodes declare them, without an engine and on the
web (`aud_dsp_effects.dart` imports no `dart:ffi`):

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

The render time of a node, measured on the calling thread - in `dart test`
or in an integration test on a phone:

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

### The example app

`example/` plays a dense chord - eight saws across three octaves - through
the stereo delay, the plate or Freeverb and the limiter on the engine of
`aud_audio`, on iOS and Android: a slider for every parameter, a drive
into the limiter up to +24 dB, the render time, the xruns, the output
peak and the latency of the limiter, and a report to copy.

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

The integration test prints two lines of JSON. `EFFECTS_BENCH` holds the
bench of every node at the sample rate and the block of the device and at
48 kHz, 256 frames. `EFFECTS_CHAIN` holds the chord played live for
`AUD_MEASURE_SECONDS` (default 10), first straight to the output, then
through delay, plate and limiter driven 12 dB over full scale: callbacks,
xruns, late callbacks, the render time with and without the effects and
the output peak, which stays at the ceiling; and the latency of the
limiter, which an impulse rendered offline on the device confirms.

## Limitations

- Of the effects its description names, the package ships reverbs,
  delays, a limiter and a tremolo today; oscillators, filters, distortion
  and further modulation are planned in `aud_audio_pm`
- No Faust, no Soundpipe, no LGPL: a node is taken only with clear
  provenance (dsp-001, license-002)
- The limiter delays the signal by its look-ahead, 1.5 ms (72 frames at
  48 kHz), and reports it as its latency
- The tempo delay plays at most 4 s: divisions longer than that at the
  tempo of the transport leave the beat
- The stereo delay never decays from feedback 0.999 on

## Documentation

- [The plan of ticket 28](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/tickets/2026-10-13-28-build-reverb-delay-and-a-master-limiter.md)
  with the nodes, their defaults and the measurements
- The decisions behind the package:
  [dsp-001](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/dsp-001.md)
  (sources with clear provenance),
  [license-002](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/license-002.md)
  (MIT, no LGPL),
  [abi-001](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/abi-001.md)
  (the C ABI),
  [build-001](https://github.com/audaudio/aud_audio_pm/blob/main/doc/2026-Q4/concepts/decisions/build-001.md)
  (vendored C++ compiled by the hook)
- The C API: [`src/aud_dsp_effects.h`](src/aud_dsp_effects.h)
- The vendored code, its licenses and the audit:
  [`src/third_party/NOTICES.md`](src/third_party/NOTICES.md)
- The guides in [`doc/guides`](doc/guides)

## How it works

Every node renders its input in place, takes any block up to the prepared
maximum and ramps its parameters through the engine's parameter events
(`AudNodeBase` of `aud_audio_core`). Buffers are allocated in `prepare`;
`process`, `reset` and `set_param` neither allocate nor lock. The reverbs
and delays take a mono or a stereo input - a mono input feeds both sides -
and render stereo; `mix` crossfades linearly from dry (0) to wet (1).
Feedback paths flush values below 1e-15 to zero, so long tails never slow
down on denormals.

| Node | Parameters (range, default) | Latency, tail | Source |
| --- | --- | --- | --- |
| `aud.effects.freeverb` | `room_size` (0..1, 0.5), `damping` (0..1, 0.5), `width` (0..1, 1), `mix` (0..1, 0.25) | 0; until the longest comb decays by 60 dB, at most 30 s | Freeverb by Jezar at Dreampoint, public domain; tunings scaled from 44.1 kHz |
| `aud.effects.plate` | `pre_delay` (s, 0..0.25, 0.02), `bandwidth` (0..1, 0.9995), `decay` (0..0.99, 0.5), `damping` (0..1, 0.0005), `mix` (0..1, 0.25) | 0; until the tank decays by 60 dB, at most 30 s | Own code from J. Dattorro, "Effect Design, Part 1", JAES 45(9), 1997; delays scaled from 29761 Hz |
| `aud.effects.stereo_delay` | `time` (s, 0.001..2, 0.4), `feedback` (0..1, 0.3), `mix` (0..1, 0.25), `ping_pong` (0/1, 0) | 0; until the echoes decay by 60 dB, endless from feedback 0.999 | DunneAudioKit's StereoDelay, MIT, Copyright (c) 2021 AudioKit; mix no longer inverted |
| `aud.effects.tempo_delay` | `division` (0..15, 8 = 1/8D), `feedback` (0..1, 0.3), `mix` (0..1, 0.25), `ping_pong` (0/1, 0) | 0; as the stereo delay | The stereo delay's lines; time = beats(division) · 60 / tempo of the transport (120 BPM without one), up to 4 s; longer divisions play 4 s and leave the beat |
| `aud.effects.limiter` | `ceiling` (dBFS, -24..0, -1), `release` (s, 0.001..2, 0.1), `gain` (dB, -24..24, 0) | 1.5 ms (72 frames at 48 kHz) for both | Own code after Giannoulis, Massberg, Reiss, JAES 60(6), 2012, and Zoelzer, DAFX ch. 4 |
| `aud.effects.tremolo` | `rate` (Hz, 0.1..20, 5), `depth` (0..1, 0.5) | 0; 0 | Own code |

The delays glide to a new time with a time constant of 50 ms, like a tape,
instead of clicking. The tempo delay reads the tempo of the segment at the
start of every block; its divisions are sorted by length: 1/32, 1/16T,
1/32D, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1/1T,
1/2D, 1/1.

The limiter applies the input gain, takes the gain each frame needs to
stay below the ceiling - linked over all channels -, holds its minimum over
the look-ahead of L frames, lets a release follower raise it no faster than
the release, smooths it with a box filter of L + 1 frames and applies it to
the signal delayed by L frames, so every peak meets the fully reduced gain;
a final clamp to the ceiling guards the rounding. Samples that are not
finite - no number or infinite, also after the gain - count as silence.

The bench (`testing.dart`) registers the package with a host of its own,
runs one node through its vtable and times each process call of a dense
chord at -6 dBFS that decays into silence every 4 seconds. On an Apple M5
at 48 kHz, 256 frames per block, the mean per block is about 4 µs for
Freeverb, 3.5 µs for the plate, 2 µs for each delay, 1 µs for the limiter
and 0.6 µs for the tremolo - well below 0.1 % of the block.

### The Dart API

- `aud_dsp_effects.dart` is platform-neutral and compiles for the web: one
  class per node - `AudFreeverb`, `AudPlateReverb`, `AudStereoDelay`,
  `AudTempoDelay`, `AudLimiter`, `AudTremolo` - holds its `typeId`, its
  `version`, the parameter ids, `params` in the order of their indices and
  `paramDescriptors` with ranges and defaults. `AudTempoDelay` adds
  `divisions`, `divisionBeats` and `maxSeconds`, `AudLimiter` adds
  `lookaheadSeconds` and `lookaheadFrames(sampleRate)`.
  `audDspEffectsVersion` is the version of the package.
- `aud_dsp_effects_ffi.dart` adds `AudDspEffects`: `name` and `register` -
  the register function - for the package list of an engine;
  `registerWith` registers with the host api of a graph and throws an
  `AudDspEffectsException` with the result `code` and the `operation` when
  the engine refuses.
- `register`, `registerWith` and `AudEffectsBench.measure` are recorded
  with `@RecordUse`: in a profile or release build the link hook keeps the
  native library only when the app uses one of them.
- `testing.dart` adds `AudEffectsBench.measure`, which returns an
  `AudEffectsBenchResult`: the mean, the p99 and the maximum in
  nanoseconds, their shares of the block and `toJson()`.

## Status

Before 1.0 — the API may change between minor versions. The package ships the nodes of ticket 28 (step
S10a); the other effects of its description are planned in `aud_audio_pm`.

## Contributing

Work is done ticket by ticket with the `gg` workflow; see the
[Develop Guide](doc/guides/develop-guide.md) and the
[Review Guide](doc/guides/for-ai/ai-review-guide.md). Every commit passes
`dart analyze`, `dart format`, `dart test` with 100 % coverage of `lib/`,
the native tests and the notices check.

## License

MIT — see [LICENSE](LICENSE). The vendored code under `src/third_party`
keeps its own licenses; [`src/third_party/NOTICES.md`](src/third_party/NOTICES.md)
names them and the audit of every node's origin.
