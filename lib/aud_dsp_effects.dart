// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

/// The effect nodes of the Audanika Audio Engine, platform-neutral: the
/// type ids, parameter ids, ranges and defaults of Freeverb, the plate
/// reverb, the stereo and the tempo delay, the limiter and the tremolo.
/// Imports no `dart:ffi`, so it compiles for the web (web-001);
/// `aud_dsp_effects_ffi.dart` registers the nodes with a native engine.
library;

export 'src/aud_dsp_effects_version.dart';
export 'src/aud_freeverb.dart';
export 'src/aud_limiter.dart';
export 'src/aud_plate_reverb.dart';
export 'src/aud_stereo_delay.dart';
export 'src/aud_tempo_delay.dart';
export 'src/aud_tremolo.dart';
