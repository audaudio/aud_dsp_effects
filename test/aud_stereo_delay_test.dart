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
  // with [params] set; renders an impulse on both inputs and returns the
  // output channels.
  List<Float32List> echoes(Map<String, double> params, {int frames = 24000}) {
    final graph = AudGraphFfi(
      listen: false,
      maxFrames: 128,
      inputChannels: const [2],
    );
    addTearDown(graph.dispose);
    AudDspEffects.registerWith(graph.hostApi);
    final node = graph.createNode(AudStereoDelay.typeId);
    params.forEach((id, value) => graph.setParam(node, id, value));
    graph.transaction((tx) {
      tx.connect(graph.io, node);
      tx.connect(node, graph.io);
    });
    graph.start();
    final input = Float32List(frames)..[0] = 1;
    return AudOfflineRenderer(graph).render(
      frames: frames,
      blockFrames: 128,
      inputs: [
        [input, input],
      ],
    )[0];
  }

  group('AudStereoDelay', () {
    test('follows the indices of the C header', () {
      expect(
        AudStereoDelay.typeId,
        bindings.AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID,
      );
      expect(
        AudStereoDelay.params,
        hasLength(bindings.AUD_STEREO_DELAY_NUM_PARAMS),
      );
      expect(
        {for (final (index, id) in AudStereoDelay.params.indexed) id: index},
        {
          AudStereoDelay.time: bindings.AUD_STEREO_DELAY_PARAM_TIME,
          AudStereoDelay.feedback: bindings.AUD_STEREO_DELAY_PARAM_FEEDBACK,
          AudStereoDelay.mix: bindings.AUD_STEREO_DELAY_PARAM_MIX,
          AudStereoDelay.pingPong: bindings.AUD_STEREO_DELAY_PARAM_PING_PONG,
        },
      );
    });

    test('echoes after its time with the feedback', () {
      final out = echoes({
        AudStereoDelay.time: 0.125,
        AudStereoDelay.feedback: 0.5,
        AudStereoDelay.mix: 1,
      });
      for (final channel in out) {
        expect(
          [channel[0], channel[6000], channel[12000], channel[18000]],
          [0, closeTo(1, 1e-6), closeTo(0.5, 1e-6), closeTo(0.25, 1e-6)],
        );
      }
    });

    test('crossfades from dry at mix 0 to wet at mix 1', () {
      // DunneAudioKit's mix was inverted; the port's is not.
      final out = echoes({
        AudStereoDelay.time: 0.125,
        AudStereoDelay.feedback: 0,
        AudStereoDelay.mix: 0.25,
      });
      expect(
        [out[0][0], out[0][6000]],
        [closeTo(0.75, 1e-6), closeTo(0.25, 1e-6)],
      );
    });

    test('alternates the sides in ping-pong', () {
      final out = echoes({
        AudStereoDelay.time: 0.125,
        AudStereoDelay.feedback: 0.5,
        AudStereoDelay.mix: 1,
        AudStereoDelay.pingPong: 1,
      });
      expect([out[0][6000], out[1][6000]], [closeTo(1, 1e-6), 0]);
      expect([out[0][12000], out[1][12000]], [0, closeTo(1, 1e-6)]);
    });
  });
}
