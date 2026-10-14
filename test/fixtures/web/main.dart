// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// Compiled to JavaScript by test/web/web_compile_test.dart: the
// platform-neutral library must not reach dart:ffi.

// ignore_for_file: avoid_print

import 'package:aud_dsp_effects/aud_dsp_effects.dart';

void main() {
  print(
    '${AudPlateReverb.typeId} ${AudPlateReverb.paramDescriptors.first.toJson()} '
    '${AudTempoDelay.divisions[8]} ${AudLimiter.lookaheadFrames(48000)} '
    '$audDspEffectsVersion',
  );
}
