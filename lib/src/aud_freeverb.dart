// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio_core/aud_audio_core.dart';

// #############################################################################
/// Jezar's Freeverb (public domain) as the node `aud.effects.freeverb`:
/// eight parallel combs and four allpasses in series per side, fed by the
/// sum of the input channels, with the tunings scaled from 44.1 kHz to the
/// sample rate. A mono input feeds both sides; the output is stereo.
///
/// The parameters are addressed by their ids
/// (`graph.setParam(node, AudFreeverb.roomSize, 0.8, rampFrames: 4800)`);
/// all of them ramp. The tail lasts until the longest comb has decayed by
/// 60 dB at the room size, at most 30 s.
abstract final class AudFreeverb {
  // ...........................................................................
  /// The type id of the node.
  static const String typeId = 'aud.effects.freeverb';

  /// The version of the node type.
  static const int version = 1;

  // ...........................................................................
  // Parameters

  /// The feedback of the combs, 0 to 1; default 0.5.
  static const String roomSize = 'room_size';

  /// The low-pass in the combs, 0 to 1; default 0.5.
  static const String damping = 'damping';

  /// The stereo width of the wet signal, 0 to 1; default 1.
  static const String width = 'width';

  /// The crossfade from the dry (0) to the wet signal (1); default 0.25.
  static const String mix = 'mix';

  /// The parameter ids in the order of their indices.
  static const List<String> params = [roomSize, damping, width, mix];

  /// The parameters as the node declares them, in the order of their
  /// indices: ranges, defaults and flags.
  static const List<AudParamDescriptor> paramDescriptors = [
    AudParamDescriptor(
      id: roomSize,
      name: 'Room size',
      defaultValue: 0.5,
      ramped: true,
    ),
    AudParamDescriptor(
      id: damping,
      name: 'Damping',
      defaultValue: 0.5,
      ramped: true,
    ),
    AudParamDescriptor(id: width, name: 'Width', defaultValue: 1, ramped: true),
    AudParamDescriptor(id: mix, name: 'Mix', defaultValue: 0.25, ramped: true),
  ];
}
