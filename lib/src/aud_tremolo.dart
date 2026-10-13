// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio_core/aud_audio_core.dart';

// #############################################################################
/// The node `aud.effects.tremolo`: a gain modulated by a sine LFO, 1 at the
/// peak of the LFO and `1 - depth` at its trough. A mono input feeds both
/// sides; the output is stereo.
abstract final class AudTremolo {
  // ...........................................................................
  /// The type id of the node.
  static const String typeId = 'aud.effects.tremolo';

  /// The version of the node type.
  static const int version = 1;

  // ...........................................................................
  // Parameters

  /// The rate of the LFO in Hz, 0.1 to 20; default 5.
  static const String rate = 'rate';

  /// The depth of the modulation, 0 to 1; default 0.5.
  static const String depth = 'depth';

  /// The parameter ids in the order of their indices.
  static const List<String> params = [rate, depth];

  /// The parameters as the node declares them, in the order of their
  /// indices: ranges, defaults and flags.
  static const List<AudParamDescriptor> paramDescriptors = [
    AudParamDescriptor(
      id: rate,
      name: 'Rate',
      unit: 'Hz',
      min: 0.1,
      max: 20,
      defaultValue: 5,
      ramped: true,
    ),
    AudParamDescriptor(
      id: depth,
      name: 'Depth',
      defaultValue: 0.5,
      ramped: true,
    ),
  ];
}
