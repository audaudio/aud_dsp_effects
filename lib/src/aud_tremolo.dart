// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:ffi';

import 'package:aud_audio_core/aud_audio_core.dart';

import 'aud_dsp_effects_bindings_generated.dart' as bindings;

// #############################################################################
/// The tremolo node of the package: a gain modulated by a sine LFO.
abstract final class AudTremolo {
  /// The type id to create the node with.
  static const String typeId = bindings.AUD_DSP_EFFECTS_TREMOLO_TYPE_ID;

  /// The index of the rate parameter in Hz.
  static const int rate = 0;

  /// The index of the depth parameter, 0 to 1.
  static const int depth = 1;
}

// #############################################################################
/// Thrown when the package cannot register its node types.
class AudDspEffectsException implements Exception {
  /// Creates the exception for a result [code].
  const AudDspEffectsException({required this.code});

  /// The result code, one of the `AUD_ERROR_*` constants.
  final int code;

  @override
  String toString() =>
      'AudDspEffectsException: registration failed with '
      '${AudAbi.resultName(code)}';
}

// #############################################################################
/// The registration of the package's node types with an engine.
abstract final class AudDspEffects {
  /// Registers the node types with the host api of an engine, e.g.
  /// `AudEngine.hostApi`; throws when the engine refuses them.
  static void register(Pointer<Void> hostApi) {
    final result = bindings.aud_dsp_effects_register(hostApi);
    if (result != AUD_OK) throw AudDspEffectsException(code: result);
  }
}
