// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:io';
import 'dart:math';
import 'dart:typed_data';

import 'package:aud_audio_graph/aud_audio_graph_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:test/test.dart';

// The golden renders of the effect nodes (ticket 28): each node between the
// stereo input and the output of a graph, rendered by the offline renderer
// in blocks of 128 frames at 48 kHz - an impulse and a short chord burst,
// 0.5 s, left and right. Within 1e-5 of the files in test/goldens;
// UPDATE_GOLDENS=1 writes them. The build hook compiles without
// floating-point contraction, so arm64 and x64 render the same samples.
void main() {
  const frames = 24000;
  const rate = 48000.0;

  // The input: an impulse at frame 0 and a chord burst of 0.1 s from frame
  // 4800 with fades of 5 ms; the right side's partials start at other
  // phases.
  List<Float32List> signal() {
    Float32List side(double phase) {
      final samples = Float32List(frames)..[0] = 0.8;
      for (var i = 0; i < 4800; i++) {
        final fade = min(1.0, min(i, 4800 - i) / 240);
        var value = 0.0;
        for (final (n, frequency) in [261.63, 329.63, 392.0].indexed) {
          value += sin(2 * pi * frequency * i / rate + phase * n);
        }
        samples[4800 + i] = 0.15 * fade * value;
      }
      return samples;
    }

    return [side(0), side(1.3)];
  }

  // Renders the node [typeId] with [params] and returns both channels.
  List<Float32List> render(String typeId, Map<String, double> params) {
    final graph = AudGraphFfi(
      listen: false,
      maxFrames: 128,
      inputChannels: const [2],
    );
    addTearDown(graph.dispose);
    AudDspEffects.registerWith(graph.hostApi);
    final node = graph.createNode(typeId);
    params.forEach((id, value) => graph.setParam(node, id, value));
    graph.transaction((tx) {
      tx.connect(graph.io, node);
      tx.connect(node, graph.io);
    });
    graph.start();
    return AudOfflineRenderer(graph)
        .render(frames: frames, blockFrames: 128, inputs: [signal()])[0];
  }

  // Compares [channels] with the golden [name], which UPDATE_GOLDENS=1
  // writes first.
  void compare(List<Float32List> channels, String name) {
    final file = File('test/goldens/$name.wav');
    if (Platform.environment['UPDATE_GOLDENS'] == '1') {
      file.parent.createSync(recursive: true);
      AudWavFile.write(
        file.path,
        AudWavData(channels: channels, sampleRate: rate),
      );
    }
    expect(
      file.existsSync(),
      isTrue,
      reason: 'run with UPDATE_GOLDENS=1 first',
    );
    final golden = AudWavFile.read(file.path).channels;
    expect(golden, hasLength(channels.length));
    var largest = 0.0;
    for (var c = 0; c < channels.length; c++) {
      expect(golden[c].length, channels[c].length);
      for (var i = 0; i < channels[c].length; i++) {
        largest = max(largest, (channels[c][i] - golden[c][i]).abs());
      }
    }
    expect(largest, lessThan(1e-5), reason: '$name differs by $largest');
  }

  group('golden renders', () {
    final cases = <String, (String, Map<String, double>)>{
      'freeverb': (AudFreeverb.typeId, {AudFreeverb.mix: 0.5}),
      'plate': (
        AudPlateReverb.typeId,
        {AudPlateReverb.mix: 0.5, AudPlateReverb.decay: 0.7},
      ),
      'stereo_delay': (
        AudStereoDelay.typeId,
        {
          AudStereoDelay.time: 0.1,
          AudStereoDelay.feedback: 0.5,
          AudStereoDelay.mix: 0.5,
          AudStereoDelay.pingPong: 1,
        },
      ),
      'tempo_delay': (
        AudTempoDelay.typeId,
        {
          AudTempoDelay.division: AudTempoDelay.divisions
              .indexOf('1/16')
              .toDouble(),
          AudTempoDelay.feedback: 0.5,
          AudTempoDelay.mix: 0.5,
        },
      ),
      'limiter': (
        AudLimiter.typeId,
        {AudLimiter.gain: 12, AudLimiter.release: 0.05},
      ),
      'tremolo': (
        AudTremolo.typeId,
        {AudTremolo.rate: 8, AudTremolo.depth: 0.8},
      ),
    };
    for (final MapEntry(key: name, value: (typeId, params)) in cases.entries) {
      test(name, () {
        final channels = render(typeId, params);
        compare(channels, name);
        // Something sounds on both sides, and nothing is out of bounds.
        for (final channel in channels) {
          final loudest = channel.map((x) => x.abs()).reduce(max);
          expect(loudest, inExclusiveRange(0.05, 2));
        }
      });
    }
  });
}
