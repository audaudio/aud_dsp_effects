// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:typed_data';

import 'package:aud_audio_graph/aud_audio_graph_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:aud_dsp_effects/src/aud_dsp_effects_bindings_generated.dart'
    as bindings;
import 'package:test/test.dart';

void main() {
  // The node between the stereo input and the output of a graph at 48 kHz,
  // with [params] set; disposed at the end of the test.
  (AudGraphFfi, AudNode) freeverb([Map<String, double> params = const {}]) {
    final graph = AudGraphFfi(
      listen: false,
      maxFrames: 128,
      inputChannels: const [2],
    );
    addTearDown(graph.dispose);
    AudDspEffects.registerWith(graph.hostApi);
    final node = graph.createNode(AudFreeverb.typeId);
    params.forEach((id, value) => graph.setParam(node, id, value));
    graph.transaction((tx) {
      tx.connect(graph.io, node);
      tx.connect(node, graph.io);
    });
    graph.start();
    return (graph, node);
  }

  Float32List impulse(int frames) => Float32List(frames)..[0] = 1;

  group('AudFreeverb', () {
    test('follows the indices of the C header', () {
      expect(AudFreeverb.typeId, bindings.AUD_DSP_EFFECTS_FREEVERB_TYPE_ID);
      expect(AudFreeverb.params, hasLength(bindings.AUD_FREEVERB_NUM_PARAMS));
      expect(
        {for (final (index, id) in AudFreeverb.params.indexed) id: index},
        {
          AudFreeverb.roomSize: bindings.AUD_FREEVERB_PARAM_ROOM_SIZE,
          AudFreeverb.damping: bindings.AUD_FREEVERB_PARAM_DAMPING,
          AudFreeverb.width: bindings.AUD_FREEVERB_PARAM_WIDTH,
          AudFreeverb.mix: bindings.AUD_FREEVERB_PARAM_MIX,
        },
      );
    });

    test('answers an impulse with a stereo reverb', () {
      final (graph, _) = freeverb({AudFreeverb.mix: 1});
      final input = impulse(24000);
      final out = AudOfflineRenderer(graph).render(
        frames: 24000,
        blockFrames: 128,
        inputs: [
          [input, input],
        ],
      )[0];
      // Fully wet: no dry impulse, a reverb after it, different per side.
      expect(out[0][0], 0);
      expect(out[0].sublist(4800).any((x) => x.abs() > 1e-3), isTrue);
      var difference = 0.0;
      for (var i = 0; i < 24000; i++) {
        difference += (out[0][i] - out[1][i]).abs();
      }
      expect(difference, greaterThan(0.1));
    });

    test('passes the dry signal at mix 0', () {
      final (graph, _) = freeverb({AudFreeverb.mix: 0});
      final input = Float32List.fromList([
        for (var i = 0; i < 4800; i++) (i % 97) / 97 - 0.5,
      ]);
      final out = AudOfflineRenderer(graph).render(
        frames: 4800,
        inputs: [
          [input, input],
        ],
      )[0];
      expect(out[0], input);
      expect(out[1], input);
    });

    test('rings longer in a larger room', () {
      final (graph, node) = freeverb();
      final small = graph.outputTail;
      graph.setParam(node, AudFreeverb.roomSize, 0.9);
      AudOfflineRenderer(graph).render(frames: 128);
      graph.transaction((_) {});
      expect(graph.outputTail, greaterThan(small));
    });
  });
}
