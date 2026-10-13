# Third-party notices of aud_dsp_effects

`aud_dsp_effects` is published under the MIT license with Audanika as the
copyright holder (decision license-002); see `LICENSE` in the root of the
package. It vendors two third-party components, compiled by the package's
build hook (decision build-001). Both were vendored on 2026-10-13 for ticket
28 of aud_audio_pm (step S10a):

- `freeverb` holds the `Components` folder of Freeverb by Jezar at
  Dreampoint from https://github.com/sinshu/freeverb, commit
  `cfcea55553fb59ac57ebf2a237f72cad4296f2b0` (2021-05-05, "Add files."), a
  copy of the original release, with its `readme.txt`, which places the code
  in the public domain.
- `dunne_audio_kit` holds `StereoDelay.h`, `StereoDelay.cpp`,
  `AdjustableDelayLine.h` and `AdjustableDelayLine.cpp` of
  `Sources/CDunneAudioKit/DunneCore/Modulated Delay` and the `LICENSE` of
  https://github.com/AudioKit/DunneAudioKit, commit
  `be581d08158ea4556a54298f75814d23ff7db886` (2026-04-15). Its
  `Sources/CDunneAudioKit/StereoDelayDSP.mm` served as the reference for the
  parameters - the time in seconds up to 2 s, the feedback, the dry/wet mix,
  the ping-pong mode - and is not vendored.

Every vendored file keeps its original header; each changed file says what
changed in a "Modified by Audanika for aud_dsp_effects" note below it.

| Component | Path | License |
| --- | --- | --- |
| Freeverb | `freeverb` | public domain, header of the file and `freeverb/readme.txt` |
| DunneAudioKit StereoDelay | `dunne_audio_kit` | MIT, Copyright (c) 2021 AudioKit, `dunne_audio_kit/LICENSE` |

## The license audit of the ported sources

Done for ticket 28 (decisions dsp-001, license-001, license-002 and
license-003):

- `freeverb/allpass.hpp`, `allpass.cpp`, `comb.hpp`, `comb.cpp`,
  `denormals.h`, `revmodel.hpp`, `revmodel.cpp`, `tuning.h`: written by
  Jezar at Dreampoint, June 2000; each header says "This code is public
  domain", and `readme.txt` gives the code away for any use, commercial
  included. Changed: the tunings scaled from 44.1 kHz to the sample rate and
  the buffers allocated by the node in prepare instead of fixed arrays
  (`revmodel`), a per-frame wet output (`revmodel::processframe`), the
  filters starting without a buffer (`comb.cpp`, `allpass.cpp`), and
  `undenormalise` as a function that reads the bits through `memcpy` - the
  original macro read a float through an `unsigned int` pointer, undefined
  behaviour under the strict aliasing rule (`denormals.h`). `allpass.hpp`,
  `comb.hpp` and `tuning.h` are unchanged.
- `dunne_audio_kit/StereoDelay.h`, `StereoDelay.cpp`,
  `AdjustableDelayLine.h`, `AdjustableDelayLine.cpp`: AudioKit's own code
  (header "Copyright AudioKit. All Rights Reserved."), released under the
  MIT license of the repository, Copyright (c) 2021 AudioKit; not ported
  from Csound, Soundpipe or Faust. Changed: the inverted dry/wet mix fixed
  (the original rendered `(1 - mix) * wet + mix * dry`, so mix 1 was dry;
  the port renders `(1 - mix) * dry + mix * wet`), a per-frame render with
  the delay set in frames, the read position in double precision computed
  per frame instead of a float index, the delay clamped to the buffer, and
  a flush of values below 1e-15 against denormals.
- `../aud_plate_node.hpp`, `../aud_plate_node.cpp`: Audanika's own code,
  written from J. Dattorro, "Effect Design, Part 1: Reverberator and Other
  Filters", J. Audio Eng. Soc. 45(9), 1997 (figure 1, tables 1 and 2); no
  code of any implementation of the paper was used. MIT, as the package.
- `../aud_limiter_node.hpp`, `../aud_limiter_node.cpp`: Audanika's own
  code after D. Giannoulis, M. Massberg and J. D. Reiss, "Digital Dynamic
  Range Compressor Design - A Tutorial and Analysis", J. Audio Eng. Soc.
  60(6), 2012, and U. Zoelzer, DAFX, chapter 4. MIT, as the package.
- The other nodes - the tremolo, the tempo delay's logic over the vendored
  delay lines - and the bench are Audanika's own code. MIT, as the package.
- No Faust-generated code, no Soundpipe module and no code under the LGPL
  or the GPL is part of the package.
