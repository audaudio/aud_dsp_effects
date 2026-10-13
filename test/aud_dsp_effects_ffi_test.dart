// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:ffi';

import 'package:aud_audio_core/aud_audio_core.dart';
import 'package:aud_audio_core/aud_audio_core_bindings.dart' as core;
import 'package:aud_audio_graph/aud_audio_graph_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:test/test.dart';

void main() {
  // A graph of the engine with the effects registered, disposed at the end
  // of the test.
  AudGraphFfi effectsGraph() {
    final graph = AudGraphFfi(listen: false, maxFrames: 256);
    addTearDown(graph.dispose);
    AudDspEffects.registerWith(graph.hostApi);
    return graph;
  }

  // The node types of the package with their Dart mirrors.
  final nodes = [
    (
      typeId: AudFreeverb.typeId,
      version: AudFreeverb.version,
      params: AudFreeverb.params,
      descriptors: AudFreeverb.paramDescriptors,
    ),
    (
      typeId: AudPlateReverb.typeId,
      version: AudPlateReverb.version,
      params: AudPlateReverb.params,
      descriptors: AudPlateReverb.paramDescriptors,
    ),
    (
      typeId: AudStereoDelay.typeId,
      version: AudStereoDelay.version,
      params: AudStereoDelay.params,
      descriptors: AudStereoDelay.paramDescriptors,
    ),
    (
      typeId: AudTempoDelay.typeId,
      version: AudTempoDelay.version,
      params: AudTempoDelay.params,
      descriptors: AudTempoDelay.paramDescriptors,
    ),
    (
      typeId: AudLimiter.typeId,
      version: AudLimiter.version,
      params: AudLimiter.params,
      descriptors: AudLimiter.paramDescriptors,
    ),
    (
      typeId: AudTremolo.typeId,
      version: AudTremolo.version,
      params: AudTremolo.params,
      descriptors: AudTremolo.paramDescriptors,
    ),
  ];

  group('AudDspEffects', () {
    test('registers with an engine once', () {
      final graph = effectsGraph();
      expect(AudDspEffects.name, 'aud_dsp_effects');
      for (final node in nodes) {
        expect(graph.nodeType(node.typeId), isNotNull, reason: node.typeId);
      }
      expect(
        () => AudDspEffects.registerWith(graph.hostApi),
        throwsA(
          isA<AudDspEffectsException>()
              .having((e) => e.code, 'code', AUD_ERROR_DUPLICATE_TYPE)
              .having((e) => e.operation, 'operation', 'registration')
              .having(
                (e) => e.toString(),
                'text',
                'AudDspEffectsException: registration failed with '
                    'AUD_ERROR_DUPLICATE_TYPE',
              ),
        ),
      );
    });

    test('hands its register function to an engine', () {
      final graph = AudGraphFfi(listen: false, maxFrames: 256);
      addTearDown(graph.dispose);
      final register = AudDspEffects.register
          .asFunction<int Function(Pointer<core.AudHostApi>)>();
      expect(register(graph.hostApi), AUD_OK);
      expect(graph.nodeType(AudPlateReverb.typeId), isNotNull);
      expect(register(nullptr), AUD_ERROR_INVALID_ARGUMENT);
    });

    test('declares the nodes the Dart constants name', () {
      final graph = effectsGraph();
      for (final node in nodes) {
        final declared = graph.nodeType(node.typeId)!;
        final reason = node.typeId;
        expect(declared.version, node.version, reason: reason);
        expect(declared.vendor, 'Audanika', reason: reason);
        expect(declared.abiMajor, AudAbi.major, reason: reason);
        expect(declared.abiMinor, AudAbi.minor, reason: reason);
        expect(declared.params.map((p) => p.id), node.params, reason: reason);
        expect(node.descriptors.map((p) => p.id), node.params, reason: reason);
        // The C table holds floats; the mirrors hold the doubles they come
        // from.
        for (final (index, mirror) in node.descriptors.indexed) {
          final param = declared.params[index];
          final where = '${node.typeId} ${mirror.id}';
          expect(param.name, mirror.name, reason: where);
          expect(param.unit, mirror.unit, reason: where);
          expect(param.min, closeTo(mirror.min, 1e-6), reason: where);
          expect(param.max, closeTo(mirror.max, 1e-6), reason: where);
          expect(
            param.defaultValue,
            closeTo(mirror.defaultValue, 1e-6),
            reason: where,
          );
          expect(param.flags, mirror.flags, reason: where);
          expect(param.steps, mirror.steps, reason: where);
        }
        final capabilities = declared.capabilities;
        expect(capabilities.inPlace, isTrue, reason: reason);
        expect(capabilities.variableBlock, isTrue, reason: reason);
        expect(capabilities.events, isTrue, reason: reason);
        expect(capabilities.tail, isTrue, reason: reason);
        expect(capabilities.strings, isFalse, reason: reason);
        expect(capabilities.state, isFalse, reason: reason);
        expect(
          capabilities.latency,
          node.typeId == AudLimiter.typeId,
          reason: reason,
        );
        expect(
          capabilities.transport,
          node.typeId == AudTempoDelay.typeId,
          reason: reason,
        );
        final limiter = node.typeId == AudLimiter.typeId;
        final input = declared.inputBuses.single;
        final output = declared.outputBuses.single;
        expect(
          (
            input.id,
            input.minChannels,
            input.maxChannels,
            input.defaultChannels,
          ),
          ('in', 1, limiter ? 16 : 2, 2),
          reason: reason,
        );
        expect(
          (
            output.id,
            output.minChannels,
            output.maxChannels,
            output.defaultChannels,
          ),
          ('out', limiter ? 1 : 2, limiter ? 16 : 2, 2),
          reason: reason,
        );
      }
    });
  });

  group('AudDspEffectsException', () {
    test('names the operation that failed', () {
      expect(
        const AudDspEffectsException(
          code: AUD_ERROR_NOT_FOUND,
          operation: 'the bench of aud.effects.plate',
        ).toString(),
        'AudDspEffectsException: the bench of aud.effects.plate failed with '
        'AUD_ERROR_NOT_FOUND',
      );
    });
  });
}
