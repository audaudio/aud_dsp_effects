// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio_core/aud_audio_core.dart';

// #############################################################################
/// A look-ahead peak limiter as the node `aud.effects.limiter`: no output
/// sample exceeds [ceiling], for any input. The gain follows the minimum of
/// the required gain over the look-ahead, a release and a box filter, linked
/// over all channels, and the signal is delayed by the look-ahead, so that
/// every peak meets the fully reduced gain. 1 to 16 channels; a mono input
/// feeds every output channel.
///
/// The look-ahead of [lookaheadSeconds] is fixed when the node is prepared
/// and reported as its latency and its tail
/// ([lookaheadFrames] at the sample rate).
abstract final class AudLimiter {
  /// The type id of the node.
  static const String typeId = 'aud.effects.limiter';

  /// The version of the node type.
  static const int version = 1;

  // ...........................................................................
  // Parameters

  /// The largest output in dBFS, -24 to 0; default -1.
  static const String ceiling = 'ceiling';

  /// The time constant in which the gain recovers in seconds, 0.001 to 2;
  /// default 0.1.
  static const String release = 'release';

  /// The input gain in dB, -24 to 24; default 0.
  static const String gain = 'gain';

  /// The parameter ids in the order of their indices.
  static const List<String> params = [ceiling, release, gain];

  /// The parameters as the node declares them, in the order of their
  /// indices: ranges, defaults and flags.
  static const List<AudParamDescriptor> paramDescriptors = [
    AudParamDescriptor(
      id: ceiling,
      name: 'Ceiling',
      unit: 'dB',
      min: -24,
      max: 0,
      defaultValue: -1,
      ramped: true,
    ),
    AudParamDescriptor(
      id: release,
      name: 'Release',
      unit: 's',
      min: 0.001,
      max: 2,
      defaultValue: 0.1,
      ramped: true,
    ),
    AudParamDescriptor(
      id: gain,
      name: 'Gain',
      unit: 'dB',
      min: -24,
      max: 24,
      ramped: true,
    ),
  ];

  // ...........................................................................
  // Look-ahead

  /// The look-ahead in seconds.
  static const double lookaheadSeconds = 0.0015;

  /// The look-ahead in frames at [sampleRate], at least 1: the latency of
  /// the node.
  static int lookaheadFrames(double sampleRate) {
    final frames = (lookaheadSeconds * sampleRate).round();
    return frames < 1 ? 1 : frames;
  }
}
