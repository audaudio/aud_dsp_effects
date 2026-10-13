// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio_core/aud_audio_core.dart';

// #############################################################################
/// DunneAudioKit's StereoDelay (MIT, Copyright (c) 2021 AudioKit) as the
/// node `aud.effects.stereo_delay`: the time in seconds, the feedback, the
/// mix - 0 dry, 1 wet, where the original was inverted - and a ping-pong
/// mode in which the echoes alternate between the sides. A mono input feeds
/// both sides; the output is stereo.
///
/// The time glides towards a new value with a time constant of 50 ms, so
/// that a jump moves the echoes like a tape instead of clicking. The tail
/// lasts until the echoes have decayed by 60 dB, at most 30 s; a feedback
/// of 0.999 and more never ends.
abstract final class AudStereoDelay {
  /// The type id of the node.
  static const String typeId = 'aud.effects.stereo_delay';

  /// The version of the node type.
  static const int version = 1;

  // ...........................................................................
  // Parameters

  /// The delay in seconds, 0.001 to 2; default 0.4.
  static const String time = 'time';

  /// The share of an echo that comes back, 0 to 1; default 0.3.
  static const String feedback = 'feedback';

  /// The crossfade from the dry (0) to the wet signal (1); default 0.25.
  static const String mix = 'mix';

  /// Whether the echoes alternate between the sides, 0 or 1; default 0.
  static const String pingPong = 'ping_pong';

  /// The parameter ids in the order of their indices.
  static const List<String> params = [time, feedback, mix, pingPong];

  /// The parameters as the node declares them, in the order of their
  /// indices: ranges, defaults and flags.
  static const List<AudParamDescriptor> paramDescriptors = [
    AudParamDescriptor(
      id: time,
      name: 'Time',
      unit: 's',
      min: 0.001,
      max: 2,
      defaultValue: 0.4,
      ramped: true,
    ),
    AudParamDescriptor(
      id: feedback,
      name: 'Feedback',
      defaultValue: 0.3,
      ramped: true,
    ),
    AudParamDescriptor(id: mix, name: 'Mix', defaultValue: 0.25, ramped: true),
    AudParamDescriptor(id: pingPong, name: 'Ping-pong', boolean: true),
  ];
}
