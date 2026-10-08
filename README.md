# aud_dsp_effects

Effects of the Audanika Audio Engine: reverbs, delays, oscillators, filters, dynamics, modulation, distortion.

Part of the Audanika Audio Engine; planned in [aud_audio_pm](https://github.com/audaudio/aud_audio_pm).

## The spike node (ticket 5)

The package builds its C++ with its own hook against the header-only ABI
of `aud_audio_core` and registers its node types with an engine at run
time. The first node is the tremolo `aud.effects.tremolo` with the
parameters `rate` (Hz) and `depth` (0 to 1).

```dart
import 'package:aud_dsp_effects/aud_dsp_effects.dart';

AudDspEffects.register(engine.hostApi);       // throws AudDspEffectsException
final tremolo = engine.createNode(AudTremolo.typeId);
engine.setParam(tremolo, AudTremolo.rate, 5);
engine.setParam(tremolo, AudTremolo.depth, 0.5);
```

Reverb and delay follow with ticket S10a.
