# Changelog

## 0.3.0 - 2026-10-28

### Changed

- Require Dart 3.13 and upgrade the dependencies

- Require Dart 3.13

- Upgrade dna_audanika to ^0.1.0

- Upgrade helix to ^1.14.0

- Set the identity the DNA layers ask for in dna/_vars.json

- Reformat 3 files with the Dart 3.13 formatter

- Add the blog post of this change, in English and German

- Bring the README to the README template, in English and German

- Upgrade the dependencies published since the first pass

- Lift the lock file to the newest resolvable versions

- Resolve the example together with the package

- Add the example to a pub workspace, resolution: workspace in the example

- Drop the lock file of the example, the package lock covers it

- Keep test within the test_api that flutter_test pins

## 0.2.4 - 2026-10-24

### Changed

- Run the native tests on Windows

## 0.2.3 - 2026-10-22

## 0.2.2 - 2026-10-15

## 0.2.1 - 2026-10-14

### Changed

- S9b: Limit the voices of the sampler by the CPU power of the device

## 0.2.0 - 2026-10-14

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
- Add the limit of the tempo delay to its documentation
- Divisions longer than 4 s, e.g. 1/1 below 60 BPM, play 4 s and leave the beat
- Add review fixes: one plate read and the Dart API in README
- Read the interpolated frame of the plate's pre-delay and modulated allpasses through one AudPlateDelay::read; the goldens are unchanged
- Take the longest time of the stereo delay from its parameter descriptor instead of a second 2.0
- Split the bench into chordAt, benchHost and summarize, so no loop nests four deep
- Fix the comments of the limiter's slidingMinimum and of the parameter flags
- Add the comment landmarks of the code guide and put methods before fields in AudEffectsBenchResult, AudDspEffectsException and the example's EffectsChain
- Say that only register and registerWith of AudDspEffects are recorded; the bench example shows 3.5 us like the README
- Test that the tempo delay plays at most AudTempoDelay.maxSeconds for a longer division
- Group the tests of AudDspEffects, AudDspEffectsException and AudEffectsBenchResult by the member they cover
- Read the sources asynchronously in test/hook/link_test.dart
- Describe the Dart API in README.md and README.de.md; mix is a linear crossfade and non-finite samples count as silence in both
- Name aud_audio as the dependency of the example in index.jsonc
- Separate the license header of the example's integration test and driver by a blank line

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
