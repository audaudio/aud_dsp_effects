// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio_core/aud_audio_core.dart';

// #############################################################################
/// The plate reverb of J. Dattorro, "Effect Design, Part 1", JAES 45(9),
/// 1997, as the node `aud.effects.plate`: pre-delay, input bandwidth, four
/// input diffusers and the figure-eight tank with two modulated allpasses,
/// the delays scaled from 29761 Hz to the sample rate. Written from the
/// paper. A mono input feeds both sides; the output is stereo.
///
/// The parameters are addressed by their ids
/// (`graph.setParam(node, AudPlateReverb.decay, 0.6, rampFrames: 4800)`);
/// all of them ramp. The tail lasts until the tank has decayed by 60 dB at
/// the decay, at most 30 s.
abstract final class AudPlateReverb {
  // ...........................................................................
  /// The type id of the node.
  static const String typeId = 'aud.effects.plate';

  /// The version of the node type.
  static const int version = 1;

  // ...........................................................................
  // Parameters

  /// The delay before the reverb in seconds, 0 to 0.25; default 0.02.
  static const String preDelay = 'pre_delay';

  /// The input low-pass, 0 to 1 (1 passes all); default 0.9995, the paper's.
  static const String bandwidth = 'bandwidth';

  /// The gain of the tank, 0 to 0.99; default 0.5, the paper's.
  static const String decay = 'decay';

  /// The low-pass in the tank, the paper's one-pole coefficient, 0 (none)
  /// to 1; default 0.0005, the paper's.
  static const String damping = 'damping';

  /// The crossfade from the dry (0) to the wet signal (1); default 0.25.
  static const String mix = 'mix';

  /// The parameter ids in the order of their indices.
  static const List<String> params = [preDelay, bandwidth, decay, damping, mix];

  /// The parameters as the node declares them, in the order of their
  /// indices: ranges, defaults and flags.
  static const List<AudParamDescriptor> paramDescriptors = [
    AudParamDescriptor(
      id: preDelay,
      name: 'Pre-delay',
      unit: 's',
      max: 0.25,
      defaultValue: 0.02,
      ramped: true,
    ),
    AudParamDescriptor(
      id: bandwidth,
      name: 'Bandwidth',
      defaultValue: 0.9995,
      ramped: true,
    ),
    AudParamDescriptor(
      id: decay,
      name: 'Decay',
      max: 0.99,
      defaultValue: 0.5,
      ramped: true,
    ),
    AudParamDescriptor(
      id: damping,
      name: 'Damping',
      defaultValue: 0.0005,
      ramped: true,
    ),
    AudParamDescriptor(id: mix, name: 'Mix', defaultValue: 0.25, ramped: true),
  ];
}
