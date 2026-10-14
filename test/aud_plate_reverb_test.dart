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
  (AudGraphFfi, AudNode) plate([Map<String, double> params = const {}]) {
    final graph = AudGraphFfi(
      listen: false,
      maxFrames: 128,
      inputChannels: const [2],
    );
    addTearDown(graph.dispose);
    AudDspEffects.registerWith(graph.hostApi);
    final node = graph.createNode(AudPlateReverb.typeId);
    params.forEach((id, value) => graph.setParam(node, id, value));
    graph.transaction((tx) {
      tx.connect(graph.io, node);
      tx.connect(node, graph.io);
    });
    graph.start();
    return (graph, node);
  }

  // Renders an impulse on both inputs and returns the output channels.
  List<Float32List> impulseResponse(AudGraphFfi graph, int frames) {
    final input = Float32List(frames)..[0] = 1;
    return AudOfflineRenderer(graph).render(
      frames: frames,
      blockFrames: 128,
      inputs: [
        [input, input],
      ],
    )[0];
  }

  group('AudPlateReverb', () {
    test('follows the indices of the C header', () {
      expect(AudPlateReverb.typeId, bindings.AUD_DSP_EFFECTS_PLATE_TYPE_ID);
      expect(AudPlateReverb.params, hasLength(bindings.AUD_PLATE_NUM_PARAMS));
      expect(
        {for (final (index, id) in AudPlateReverb.params.indexed) id: index},
        {
          AudPlateReverb.preDelay: bindings.AUD_PLATE_PARAM_PRE_DELAY,
          AudPlateReverb.bandwidth: bindings.AUD_PLATE_PARAM_BANDWIDTH,
          AudPlateReverb.decay: bindings.AUD_PLATE_PARAM_DECAY,
          AudPlateReverb.damping: bindings.AUD_PLATE_PARAM_DAMPING,
          AudPlateReverb.mix: bindings.AUD_PLATE_PARAM_MIX,
        },
      );
    });

    test('starts its reverb after the pre-delay', () {
      final (graph, _) = plate({
        AudPlateReverb.mix: 1,
        AudPlateReverb.preDelay: 0.1,
      });
      final out = impulseResponse(graph, 24000);
      for (final channel in out) {
        expect(channel.sublist(0, 4800).every((x) => x == 0), isTrue);
        expect(channel.sublist(4800).any((x) => x.abs() > 1e-3), isTrue);
      }
    });

    test('decays slower with a larger decay', () {
      // The energy of the second half of a second against the first.
      double ratio(double decay) {
        final (graph, _) = plate({
          AudPlateReverb.mix: 1,
          AudPlateReverb.decay: decay,
        });
        final left = impulseResponse(graph, 48000)[0];
        var early = 0.0;
        var late = 0.0;
        for (var i = 0; i < 48000; i++) {
          if (i < 24000) {
            early += left[i] * left[i];
          } else {
            late += left[i] * left[i];
          }
        }
        return late / early;
      }

      expect(ratio(0.8), greaterThan(ratio(0.3)));
    });
  });
}
