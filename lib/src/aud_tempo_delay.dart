// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio_core/aud_audio_core.dart';

// #############################################################################
/// The delay lines of `AudStereoDelay` with the time of a note value at the
/// tempo of the transport, as the node `aud.effects.tempo_delay`: the time
/// is `divisionBeats[division] * 60 / tempo` of the segment at the start of
/// each block - 120 BPM without a transport -, at most 4 s, and glides when
/// the tempo or the division changes. A mono input feeds both sides; the
/// output is stereo.
abstract final class AudTempoDelay {
  // ...........................................................................
  /// The type id of the node.
  static const String typeId = 'aud.effects.tempo_delay';

  /// The version of the node type.
  static const int version = 1;

  // ...........................................................................
  // Parameters

  /// The note value, an index into [divisions], stepped; default 8, `1/8D`.
  static const String division = 'division';

  /// The share of an echo that comes back, 0 to 1; default 0.3.
  static const String feedback = 'feedback';

  /// The crossfade from the dry (0) to the wet signal (1); default 0.25.
  static const String mix = 'mix';

  /// Whether the echoes alternate between the sides, 0 or 1; default 0.
  static const String pingPong = 'ping_pong';

  /// The parameter ids in the order of their indices.
  static const List<String> params = [division, feedback, mix, pingPong];

  /// The parameters as the node declares them, in the order of their
  /// indices: ranges, defaults and flags.
  static const List<AudParamDescriptor> paramDescriptors = [
    AudParamDescriptor(
      id: division,
      name: 'Division',
      max: 15,
      defaultValue: 8,
      stepped: true,
      steps: 16,
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

  // ...........................................................................
  // Divisions

  /// The note values of [division] by index, sorted by length: `T` is a
  /// triplet, `D` a dotted note.
  static const List<String> divisions = [
    '1/32',
    '1/16T',
    '1/32D',
    '1/16',
    '1/8T',
    '1/16D',
    '1/8',
    '1/4T',
    '1/8D',
    '1/4',
    '1/2T',
    '1/4D',
    '1/2',
    '1/1T',
    '1/2D',
    '1/1',
  ];

  /// The length of each of [divisions] in quarter notes.
  static const List<double> divisionBeats = [
    1 / 8,
    1 / 6,
    3 / 16,
    1 / 4,
    1 / 3,
    3 / 8,
    1 / 2,
    2 / 3,
    3 / 4,
    1,
    4 / 3,
    3 / 2,
    2,
    8 / 3,
    3,
    4,
  ];

  /// The longest delay in seconds: a whole note at 60 BPM. A longer
  /// division - e.g. 1/1 below 60 BPM, 1/2D below 45 BPM - plays this time
  /// instead and leaves the beat; [divisionBeats] gives the beats to check
  /// a division against the tempo.
  static const double maxSeconds = 4;
}
