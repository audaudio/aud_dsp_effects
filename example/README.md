# aud_dsp_effects_example

A dense chord - eight saws across three octaves - through the stereo
delay, the plate or Freeverb and the limiter of `aud_dsp_effects` on the
engine of `aud_audio`, on iOS and Android:

- the engine started and stopped, its state, route and format
- the chord on and off, the effects on and off, the reverb to choose
- a slider for every parameter of the nodes, a drive into the limiter up
  to +24 dB
- the render time, the callbacks and xruns, the output peak and the
  latency of the limiter, with a report to copy

```bash
flutter run -d <device>
flutter test integration_test/effects_test.dart -d <device> \
  --dart-define=AUD_MEASURE_SECONDS=10
```

`effects_test.dart` prints `EFFECTS_BENCH` - the render time of every node
on the bench, at the sample rate and the block of the device and at
48 kHz, 256 frames - and `EFFECTS_CHAIN`: the chord live for
`AUD_MEASURE_SECONDS` (default 10) straight to the output, then through
delay, plate and limiter with a drive of +12 dB, and the latency of the
limiter, checked with an impulse rendered offline on the device.
