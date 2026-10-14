// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_dsp_effects/testing.dart';
import 'package:test/test.dart';

void main() {
  group('AudEffectsBenchResult', () {
    const result = AudEffectsBenchResult(
      typeId: 'aud.effects.plate',
      sampleRate: 48000,
      frames: 256,
      channels: 2,
      blocks: 2000,
      blockNs: 5333333.25,
      meanNs: 53333.3325,
      p99Ns: 106666.665,
      maxNs: 533333.325,
    );

    group('meanShare, p99Share, maxShare', () {
      test('share the times of the duration of a block', () {
        expect(result.meanShare, closeTo(0.01, 1e-12));
        expect(result.p99Share, closeTo(0.02, 1e-12));
        expect(result.maxShare, closeTo(0.1, 1e-12));
      });
    });

    group('toJson()', () {
      test('names every number', () {
        expect(result.toJson(), {
          'typeId': 'aud.effects.plate',
          'sampleRate': 48000.0,
          'frames': 256,
          'channels': 2,
          'blocks': 2000,
          'blockNs': 5333333.25,
          'meanNs': 53333.3325,
          'p99Ns': 106666.665,
          'maxNs': 533333.325,
          'meanShare': result.meanShare,
          'p99Share': result.p99Share,
          'maxShare': result.maxShare,
        });
      });
    });

    group('toString()', () {
      test('reads in microseconds', () {
        expect(
          result.toString(),
          'aud.effects.plate: mean 53.3 µs, p99 106.7 µs, max 533.3 µs per '
          'block of 256 frames at 48000 Hz (1.00 % of the block)',
        );
      });
    });
  });
}
