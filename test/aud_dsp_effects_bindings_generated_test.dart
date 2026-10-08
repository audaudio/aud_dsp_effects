// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:ffi';

import 'package:aud_audio_core/aud_audio_core.dart';
import 'package:aud_dsp_effects/src/aud_dsp_effects_bindings_generated.dart'
    as bindings;
import 'package:test/test.dart';

void main() {
  group('aud_dsp_effects_bindings_generated.dart', () {
    test('aud_dsp_effects_register refuses a null host', () {
      expect(
        bindings.aud_dsp_effects_register(nullptr),
        AUD_ERROR_INVALID_ARGUMENT,
      );
    });
  });
}
