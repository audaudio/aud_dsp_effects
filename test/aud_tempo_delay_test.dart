// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:typed_data';

import 'package:aud_audio_core/aud_audio_core.dart';
import 'package:aud_audio_graph/aud_audio_graph_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:aud_dsp_effects/src/aud_dsp_effects_bindings_generated.dart'
    as bindings;
import 'package:test/test.dart';

void main() {
  // The node between the stereo input and the output of a graph at 48 kHz,
  // fully wet and without feedback; disposed at the end of the test.
  (AudGraphFfi, AudNode) tempoDelay() {
    final graph = AudGraphFfi(
      listen: false,
      maxFrames: 128,
      inputChannels: const [2],
    );
    addTearDown(graph.dispose);
    AudDspEffects.registerWith(graph.hostApi);
    final node = graph.createNode(AudTempoDelay.typeId);
    graph.setParam(node, AudTempoDelay.feedback, 0);
    graph.setParam(node, AudTempoDelay.mix, 1);
    graph.transaction((tx) {
      tx.connect(graph.io, node);
      tx.connect(node, graph.io);
    });
    graph.start();
    return (graph, node);
  }

  // The frame of the first echo of an impulse within [frames], after a
  // second of silence in which the delay glides to its time.
  int echoFrame(AudGraphFfi graph, {int frames = 96000}) {
    final renderer = AudOfflineRenderer(graph);
    final silence = Float32List(48000);
    renderer.render(
      frames: 48000,
      inputs: [
        [silence, silence],
      ],
    );
    final input = Float32List(frames)..[0] = 1;
    final left = renderer.render(
      frames: frames,
      inputs: [
        [input, input],
      ],
    )[0][0];
    return left.indexWhere((x) => x > 0.5);
  }

  group('AudTempoDelay', () {
    test('follows the indices of the C header', () {
      expect(
        AudTempoDelay.typeId,
        bindings.AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID,
      );
      expect(
        AudTempoDelay.params,
        hasLength(bindings.AUD_TEMPO_DELAY_NUM_PARAMS),
      );
      expect(
        {for (final (index, id) in AudTempoDelay.params.indexed) id: index},
        {
          AudTempoDelay.division: bindings.AUD_TEMPO_DELAY_PARAM_DIVISION,
          AudTempoDelay.feedback: bindings.AUD_TEMPO_DELAY_PARAM_FEEDBACK,
          AudTempoDelay.mix: bindings.AUD_TEMPO_DELAY_PARAM_MIX,
          AudTempoDelay.pingPong: bindings.AUD_TEMPO_DELAY_PARAM_PING_PONG,
        },
      );
      expect(
        AudTempoDelay.divisions,
        hasLength(bindings.AUD_TEMPO_DELAY_NUM_DIVISIONS),
      );
      expect(
        {
          for (final (index, label) in AudTempoDelay.divisions.indexed)
            if (const ['1/32', '1/8D', '1/4', '1/1'].contains(label))
              label: index,
        },
        {
          '1/32': bindings.AUD_TEMPO_DELAY_DIVISION_32,
          '1/8D': bindings.AUD_TEMPO_DELAY_DIVISION_8D,
          '1/4': bindings.AUD_TEMPO_DELAY_DIVISION_4,
          '1/1': bindings.AUD_TEMPO_DELAY_DIVISION_1,
        },
      );
    });

    test('sorts its divisions by length', () {
      final beats = AudTempoDelay.divisionBeats;
      expect(beats, hasLength(AudTempoDelay.divisions.length));
      for (var i = 1; i < beats.length; i++) {
        expect(beats[i], greaterThan(beats[i - 1]));
      }
      final eighthDotted = AudTempoDelay.divisions.indexOf('1/8D');
      expect(beats[eighthDotted], 0.75);
      expect(AudTempoDelay.paramDescriptors.first.defaultValue, eighthDotted);
      expect(beats.last, 4);
    });

    test('delays by the division at the tempo of the transport', () {
      final (graph, node) = tempoDelay();
      // The internal transport starts at 120 BPM: a dotted eighth is 0.375 s.
      expect(echoFrame(graph), 18000);
      graph.transport(const AudTransportRequest.setTempo(150));
      expect(echoFrame(graph), 14400);
      graph.setParam(
        node,
        AudTempoDelay.division,
        AudTempoDelay.divisions.indexOf('1/4').toDouble(),
      );
      expect(echoFrame(graph), 19200);
    });

    test('plays at most maxSeconds for a longer division', () {
      final (graph, node) = tempoDelay();
      // A whole note at 30 BPM lasts 8 s; the delay plays 4 s.
      graph.transport(const AudTransportRequest.setTempo(30));
      graph.setParam(
        node,
        AudTempoDelay.division,
        AudTempoDelay.divisions.indexOf('1/1').toDouble(),
      );
      expect(
        AudTempoDelay.divisionBeats.last * 60 / 30,
        greaterThan(AudTempoDelay.maxSeconds),
      );
      expect(
        echoFrame(graph, frames: 200000),
        (AudTempoDelay.maxSeconds * 48000).round(),
      );
    });
  });
}
