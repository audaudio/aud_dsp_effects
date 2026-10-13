// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:ffi';

import 'package:aud_audio_core/aud_audio_core.dart';
import 'package:aud_audio_core/aud_audio_core_bindings.dart' as core;
import 'package:meta/meta.dart';

import 'aud_dsp_effects_bindings_generated.dart' as bindings;

// #############################################################################
/// The registration of the package's effect nodes with an engine: Freeverb,
/// the plate reverb, the stereo and the tempo delay, the limiter and the
/// tremolo.
///
/// Every member is recorded (`@RecordUse`): in a profile or release build
/// the link hook of the package keeps its native library only when an app
/// uses one of them.
///
/// ```dart
/// final engine = AudEngine(packages: [
///   AudNativeNodePackage(AudDspEffects.name, AudDspEffects.register),
/// ]);
/// ```
abstract final class AudDspEffects {
  /// The name of the package, e.g. for an engine's package list.
  static const String name = 'aud_dsp_effects';

  /// The register function of the package,
  /// `aud_dsp_effects_register(const AudHostApi*)`, which an engine calls
  /// with its host api.
  @RecordUse()
  static core.AudRegisterFunction get register =>
      Native.addressOf<NativeFunction<core.AudRegisterFunctionFunction>>(
        bindings.aud_dsp_effects_register,
      );

  /// Registers the node types with the engine behind [host], e.g.
  /// `AudGraphFfi.hostApi`; throws an [AudDspEffectsException] when the
  /// engine refuses them.
  @RecordUse()
  static void registerWith(Pointer<core.AudHostApi> host) {
    final result = bindings.aud_dsp_effects_register(host);
    if (result != AUD_OK) throw AudDspEffectsException(code: result);
  }
}

// #############################################################################
/// Thrown when an engine refuses the node types of the package, or when the
/// bench cannot measure a node.
class AudDspEffectsException implements Exception {
  /// Creates the exception for a result [code] of [operation].
  const AudDspEffectsException({
    required this.code,
    this.operation = 'registration',
  });

  /// The result code, one of the `AUD_ERROR_*` constants.
  final int code;

  /// What failed, e.g. `registration`.
  final String operation;

  @override
  String toString() =>
      'AudDspEffectsException: $operation failed with '
      '${AudAbi.resultName(code)}';
}
