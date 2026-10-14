// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:math';
import 'dart:typed_data';

import 'package:aud_audio_graph/aud_audio_graph_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:aud_dsp_effects/src/aud_dsp_effects_bindings_generated.dart'
    as bindings;
import 'package:test/test.dart';

void main() {
  // The node between the stereo input and the output of a graph at
  // [sampleRate], with [params] set; disposed at the end of the test.
  (AudGraphFfi, AudNode) limiter({
    double sampleRate = 48000,
    Map<String, double> params = const {},
  }) {
    final graph = AudGraphFfi(
      listen: false,
      sampleRate: sampleRate,
      maxFrames: 256,
      inputChannels: const [2],
    );
    addTearDown(graph.dispose);
    AudDspEffects.registerWith(graph.hostApi);
    final node = graph.createNode(AudLimiter.typeId);
    params.forEach((id, value) => graph.setParam(node, id, value));
    graph.transaction((tx) {
      tx.connect(graph.io, node);
      tx.connect(node, graph.io);
    });
    graph.start();
    return (graph, node);
  }

  // The linear ceiling of [db] as the node holds it, a float.
  double ceilingOf(double db) =>
      Float32List.fromList([pow(10, db / 20).toDouble()])[0];

  group('AudLimiter', () {
    test('follows the indices of the C header', () {
      expect(AudLimiter.typeId, bindings.AUD_DSP_EFFECTS_LIMITER_TYPE_ID);
      expect(AudLimiter.params, hasLength(bindings.AUD_LIMITER_NUM_PARAMS));
      expect(
        {for (final (index, id) in AudLimiter.params.indexed) id: index},
        {
          AudLimiter.ceiling: bindings.AUD_LIMITER_PARAM_CEILING,
          AudLimiter.release: bindings.AUD_LIMITER_PARAM_RELEASE,
          AudLimiter.gain: bindings.AUD_LIMITER_PARAM_GAIN,
        },
      );
    });

    group('lookaheadFrames(sampleRate)', () {
      test('rounds 1.5 ms to frames, at least one', () {
        expect(
          [
            for (final rate in [44100.0, 48000.0, 96000.0, 100.0])
              AudLimiter.lookaheadFrames(rate),
          ],
          [66, 72, 144, 1],
        );
      });
    });

    for (final rate in [44100.0, 48000.0, 96000.0]) {
      test('delays an impulse by exactly its latency at $rate Hz', () {
        final (graph, node) = limiter(sampleRate: rate);
        final lookahead = AudLimiter.lookaheadFrames(rate);
        expect(graph.latencyOf(node), lookahead);
        expect(graph.outputLatency, lookahead);
        final input = Float32List(4800)..[100] = 0.5;
        final out = AudOfflineRenderer(graph).render(
          frames: 4800,
          blockFrames: 100,
          inputs: [
            [input, input],
          ],
        )[0];
        for (final channel in out) {
          expect(channel.indexWhere((x) => x != 0), 100 + lookahead);
          expect(channel[100 + lookahead], 0.5);
          expect(channel.lastIndexWhere((x) => x != 0), 100 + lookahead);
        }
      });
    }

    test('keeps every sample below the ceiling, 24 dB over full scale', () {
      final random = Random(28);
      const frames = 48000;
      const hot = 15.85; // +24 dB
      final noise = Float32List.fromList([
        for (var i = 0; i < frames; i++) hot * (random.nextDouble() * 2 - 1),
      ]);
      final chord = Float32List.fromList([
        for (var i = 0; i < frames; i++)
          hot /
              3 *
              (sin(2 * pi * 220 * i / 48000) +
                  sin(2 * pi * 277 * i / 48000) +
                  sin(2 * pi * 330 * i / 48000)),
      ]);
      final spikes = Float32List(frames);
      for (var i = 50; i < frames; i += 1 + random.nextInt(300)) {
        spikes[i] = random.nextBool() ? hot : -hot;
      }
      for (final (name, left, right) in [
        ('noise', noise, chord),
        ('chord', chord, chord),
        ('spikes', spikes, Float32List(frames)),
      ]) {
        for (final ceilingDb in [-1.0, -6.0]) {
          final (graph, _) = limiter(params: {AudLimiter.ceiling: ceilingDb});
          final out = AudOfflineRenderer(graph).render(
            frames: frames,
            blockSizes: const [1, 17, 256, 100],
            inputs: [
              [left, right],
            ],
          )[0];
          final ceiling = ceilingOf(ceilingDb);
          final loudest = [
            for (final channel in out) channel.map((x) => x.abs()).reduce(max),
          ].reduce(max);
          expect(loudest, lessThanOrEqualTo(ceiling), reason: name);
          expect(loudest, greaterThan(0.5 * ceiling), reason: name);
        }
      }
    });

    test('raises the input by its gain up to the ceiling', () {
      final (graph, _) = limiter(params: {AudLimiter.gain: 6});
      final input = Float32List.fromList([
        for (var i = 0; i < 9600; i++) 0.1 * sin(2 * pi * 440 * i / 48000),
      ]);
      final out = AudOfflineRenderer(graph).render(
        frames: 9600,
        inputs: [
          [input, input],
        ],
      )[0];
      final loudest = out[0].map((x) => x.abs()).reduce(max);
      expect(loudest, closeTo(0.1 * pow(10, 6 / 20), 1e-3));
    });
  });
}
