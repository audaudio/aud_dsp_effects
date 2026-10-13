# Changelog

## Unreleased

### Added

- Add reverbs, delays and a look-ahead limiter on ABI 0.4
- Add aud.effects.freeverb on Jezar's public-domain Freeverb, tuned to the sample rate
- Add aud.effects.plate, the plate reverb of Dattorro (1997) written from the paper
- Add aud.effects.stereo_delay, ported from DunneAudioKit's StereoDelay with the mix fixed to 0 dry, 1 wet
- Add aud.effects.tempo_delay with note divisions on the tempo of the transport
- Add aud.effects.limiter: 1.5 ms look-ahead reported as latency, no sample above the ceiling
- Move the tremolo onto AudNodeBase and aud_audio_core 0.5.1
- Register through AudDspEffects.register for AudNativeNodePackage, with a link hook
- Add AudEffectsBench in testing.dart: render time per node and block
- Add the notices of the vendored sources and scripts/check-notices.js
- Add golden renders, native tests under ASan, UBSan, RTSan and TSan
- Add an example that plays a chord through the effects and measures them
- A page with a saw chord through delay, plate or Freeverb and limiter, a slider per parameter, a drive and the numbers
- integration_test/effects_test.dart: the render time of every node, the chain on the engine, the output peak and the look-ahead
- The example depends on aud_audio for the engine
- Describe the example in the READMEs

## 0.1.0 - 2026-10-08

### Added

- Add the tremolo node through the C ABI

## 0.0.2 - 2026-10-08

### Added

- Initial boilerplate

### Changed

- Record the gg commit state
- Set up GitHub repo settings and branch rules
- Update dev dependencies
