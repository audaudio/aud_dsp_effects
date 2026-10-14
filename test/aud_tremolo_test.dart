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
  group('AudTremolo', () {
    test('follows the indices of the C header', () {
      expect(AudTremolo.typeId, bindings.AUD_DSP_EFFECTS_TREMOLO_TYPE_ID);
      expect(AudTremolo.params, hasLength(bindings.AUD_TREMOLO_NUM_PARAMS));
      expect(
        {for (final (index, id) in AudTremolo.params.indexed) id: index},
        {
          AudTremolo.rate: bindings.AUD_TREMOLO_PARAM_RATE,
          AudTremolo.depth: bindings.AUD_TREMOLO_PARAM_DEPTH,
        },
      );
    });

    test('moves the gain from 1 down to 1 - depth at the rate', () {
      final graph = AudGraphFfi(
        listen: false,
        maxFrames: 128,
        inputChannels: const [2],
      );
      addTearDown(graph.dispose);
      AudDspEffects.registerWith(graph.hostApi);
      final node = graph.createNode(AudTremolo.typeId);
      graph.setParam(node, AudTremolo.rate, 2);
      graph.setParam(node, AudTremolo.depth, 0.8);
      graph.transaction((tx) {
        tx.connect(graph.io, node);
        tx.connect(node, graph.io);
      });
      graph.start();
      final ones = Float32List(48000)..fillRange(0, 48000, 1);
      final left = AudOfflineRenderer(graph).render(
        frames: 48000,
        inputs: [
          [ones, ones],
        ],
      )[0][0];
      expect(left.reduce(max), closeTo(1, 1e-4));
      expect(left.reduce(min), closeTo(0.2, 1e-4));
      // Two cycles a second: the peaks after a quarter cycle and one more.
      expect(
        [left[6000], left[18000], left[30000]],
        [closeTo(1, 1e-4), closeTo(0.2, 1e-4), closeTo(1, 1e-4)],
      );
    });
  });
}
