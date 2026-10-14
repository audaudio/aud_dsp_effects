// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio_core/aud_audio_core.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:aud_dsp_effects/testing.dart';
import 'package:test/test.dart';

void main() {
  group('AudEffectsBench', () {
    group('measure(typeId)', () {
      for (final typeId in [
        AudFreeverb.typeId,
        AudPlateReverb.typeId,
        AudStereoDelay.typeId,
        AudTempoDelay.typeId,
        AudLimiter.typeId,
        AudTremolo.typeId,
      ]) {
        test('times $typeId block by block', () {
          final result = AudEffectsBench.measure(
            typeId,
            frames: 128,
            blocks: 50,
            warmupBlocks: 5,
          );
          expect(result.typeId, typeId);
          expect(result.blocks, 50);
          expect(result.frames, 128);
          expect(result.channels, 2);
          expect(result.sampleRate, 48000);
          expect(result.blockNs, closeTo(128 / 48000 * 1e9, 1e-6));
          expect(result.meanNs, greaterThan(0));
          expect(result.meanNs, lessThanOrEqualTo(result.maxNs));
          expect(result.p99Ns, lessThanOrEqualTo(result.maxNs));
          // Far below the duration of a block, even under a debugger.
          expect(result.meanShare, lessThan(0.5));
        });
      }

      test('sets the parameters and takes a mono input', () {
        final result = AudEffectsBench.measure(
          AudPlateReverb.typeId,
          sampleRate: 96000,
          frames: 64,
          blocks: 10,
          warmupBlocks: 0,
          channels: 1,
          params: {AudPlateReverb.decay: 0.9, AudPlateReverb.mix: 1},
        );
        expect(result.blocks, 10);
        expect(result.blockNs, closeTo(64 / 96000 * 1e9, 1e-6));
      });

      test('throws for what it cannot measure', () {
        Matcher fails(int code, String operation) => throwsA(
          isA<AudDspEffectsException>()
              .having((e) => e.code, 'code', code)
              .having((e) => e.operation, 'operation', operation),
        );
        expect(
          () => AudEffectsBench.measure('aud.effects.unknown', blocks: 1),
          fails(AUD_ERROR_UNKNOWN_TYPE, 'the bench of aud.effects.unknown'),
        );
        expect(
          () => AudEffectsBench.measure(
            AudFreeverb.typeId,
            blocks: 1,
            params: {'colour': 1},
          ),
          fails(AUD_ERROR_NOT_FOUND, 'the bench of aud.effects.freeverb'),
        );
        expect(
          () => AudEffectsBench.measure(AudFreeverb.typeId, channels: 3),
          fails(
            AUD_ERROR_INVALID_ARGUMENT,
            'the bench of aud.effects.freeverb',
          ),
        );
      });
    });
  });
}
