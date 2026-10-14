// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:ffi';

import 'package:aud_audio_core/aud_audio_core.dart';
import 'package:ffi/ffi.dart';
import 'package:meta/meta.dart';

import 'aud_dsp_effects_bindings_generated.dart' as bindings;
import 'aud_dsp_effects_ffi.dart';
import 'aud_effects_bench_result.dart';

// #############################################################################
/// The render time of one node of the package (ticket 28, scope 5): runs
/// the node through its vtable on the calling thread, block by block, with
/// a stereo test signal - a dense chord at -6 dBFS that decays into silence
/// every 4 seconds - and times each process call with a steady clock. The
/// engine times only whole blocks; this measures one node, also inside an
/// integration test on a phone.
///
/// ```dart
/// final result = AudEffectsBench.measure(AudPlateReverb.typeId);
/// print(result); // aud.effects.plate: mean 3.5 µs, p99 ...
/// ```
abstract final class AudEffectsBench {
  // ...........................................................................
  /// Times the node [typeId] and returns the render time of its blocks.
  ///
  /// - [sampleRate] the sample rate in Hz, 8 to 384 kHz
  /// - [frames] the frames of a block
  /// - [blocks] the blocks that are timed
  /// - [warmupBlocks] the blocks rendered before them
  /// - [channels] the channels of the input; the limiter's output has as
  ///   many, the other nodes render stereo
  /// - [params] parameter values by id, set after prepare
  ///
  /// Throws an [AudDspEffectsException] with `AUD_ERROR_UNKNOWN_TYPE` for
  /// a type the package does not register, `AUD_ERROR_NOT_FOUND` for an
  /// unknown parameter and `AUD_ERROR_INVALID_ARGUMENT` for arguments out
  /// of range.
  @RecordUse()
  static AudEffectsBenchResult measure(
    String typeId, {
    double sampleRate = 48000,
    int frames = 256,
    int blocks = 2000,
    int warmupBlocks = 50,
    int channels = 2,
    Map<String, double> params = const {},
  }) => using((arena) {
    final request = arena<bindings.AudEffectsBenchRequest>();
    final result = arena<bindings.AudEffectsBenchResult>();
    final ids = arena<Pointer<Char>>(params.isEmpty ? 1 : params.length);
    final values = arena<Float>(params.isEmpty ? 1 : params.length);
    for (final (index, entry) in params.entries.indexed) {
      ids[index] = entry.key.toNativeUtf8(allocator: arena).cast();
      values[index] = entry.value;
    }
    request.ref
      ..struct_size = sizeOf<bindings.AudEffectsBenchRequest>()
      ..frames = frames
      ..blocks = blocks
      ..warmup_blocks = warmupBlocks
      ..channels = channels
      ..num_params = params.length
      ..sample_rate = sampleRate
      ..type_id = typeId.toNativeUtf8(allocator: arena).cast()
      ..param_ids = ids
      ..param_values = values;
    result.ref.struct_size = sizeOf<bindings.AudEffectsBenchResult>();
    final code = bindings.aud_dsp_effects_bench_run(request, result);
    if (code != AUD_OK) {
      throw AudDspEffectsException(
        code: code,
        operation: 'the bench of $typeId',
      );
    }
    return AudEffectsBenchResult(
      typeId: typeId,
      sampleRate: sampleRate,
      frames: frames,
      channels: channels,
      blocks: result.ref.blocks,
      blockNs: result.ref.block_ns,
      meanNs: result.ref.mean_ns,
      p99Ns: result.ref.p99_ns,
      maxNs: result.ref.max_ns,
    );
  });
}
