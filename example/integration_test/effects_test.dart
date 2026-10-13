// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.
// Measures the effects on a simulator, an emulator or a device (ticket 28,
// scope 6) and prints one line of JSON per measurement:
//
//   flutter test integration_test/effects_test.dart -d <device> \
//     --dart-define=AUD_MEASURE_SECONDS=10
//
// - EFFECTS_BENCH: the render time of every node of the package, timed
//   through its vtable by AudEffectsBench at the sample rate and the block
//   of the engine and at 48 kHz, 256 frames: mean, p99 and max µs per block
//   and their share of the block;
// - EFFECTS_CHAIN: the chord of the page - eight saws peaking at full scale
//   at most - live for AUD_MEASURE_SECONDS (default 10) each, first straight
//   to the output, then through stereo delay -> plate -> limiter with a
//   drive of +12 dB: callbacks, xruns, late callbacks, render time - and
//   the time the effects add - and the output peak, which stays at the
//   ceiling of -1 dBFS; the latency of the limiter, and an impulse rendered
//   offline on the device that leaves the limiter after exactly that
//   latency.
//
// The block of the engine is the mean of the blocks it rendered: a device
// may call with other blocks than the buffer it reports.
//
// A simulator or an emulator renders at the speed of its host; the numbers
// of a device come from the device.

// ignore_for_file: avoid_print

import 'dart:convert';
import 'dart:math';
import 'dart:typed_data';

import 'package:aud_audio/aud_audio_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:aud_dsp_effects/testing.dart';
import 'package:aud_dsp_effects_example/effects_chain.dart';
import 'package:aud_dsp_effects_example/effects_page.dart';
import 'package:aud_dsp_effects_example/main.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

const _seconds = int.fromEnvironment('AUD_MEASURE_SECONDS', defaultValue: 10);

// The drive into the limiter in dB: the chord 12 dB over full scale.
const _driveDb = 12.0;

// The node types the bench measures.
const _nodeTypes = [
  AudFreeverb.typeId,
  AudPlateReverb.typeId,
  AudStereoDelay.typeId,
  AudTempoDelay.typeId,
  AudLimiter.typeId,
  AudTremolo.typeId,
];

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  // Pumps the frames of the page for [duration] while the engine plays.
  Future<void> animate(WidgetTester tester, Duration duration) async {
    final end = DateTime.now().add(duration);
    while (DateTime.now().isBefore(end)) {
      await tester.pump(const Duration(milliseconds: 16));
      await tester.runAsync(
        () => Future<void>.delayed(const Duration(milliseconds: 16)),
      );
    }
  }

  // Taps the widget with [key] after scrolling it into view.
  Future<void> tap(WidgetTester tester, String key) async {
    final finder = find.byKey(Key(key));
    await tester.ensureVisible(finder);
    await tester.pump();
    await tester.tap(finder);
    await tester.pump();
  }

  // [value] rounded to [digits] decimals.
  double round(num value, int digits) =>
      double.parse(value.toStringAsFixed(digits));

  // The format of the stream as JSON.
  Map<String, Object> formatJson(AudIoStreamFormat format) => {
    'sampleRate': format.sampleRate,
    'bufferFrames': format.bufferFrames,
    'maxFrames': format.maxFrames,
    'backend': format.backend,
  };

  // Resets the statistics, plays for AUD_MEASURE_SECONDS and returns the
  // numbers of that time.
  Future<_Played> measure(WidgetTester tester, AudEngine engine) async {
    engine.graph.resetStats();
    final start = engine.numbers;
    await animate(tester, const Duration(seconds: _seconds));
    final end = engine.numbers;
    final blockFrames = EffectsPageState.blockFramesOf(engine);
    return (
      callbacks: end.callbacks - start.callbacks,
      xruns: end.xruns - start.xruns,
      lateCallbacks: end.lateCallbacks - start.lateCallbacks,
      blocks: end.blocksRendered,
      blockFrames: blockFrames,
      meanNs: end.renderTimeMeanNs,
      maxNs: end.renderTimeMaxNs,
      blockNs: blockFrames / end.sampleRate * 1e9,
      peak: engine.stats.outputPeak,
    );
  }

  // [played] as JSON.
  Map<String, Object> playedJson(_Played played) => {
    'callbacks': played.callbacks,
    'xruns': played.xruns,
    'lateCallbacks': played.lateCallbacks,
    'blocksRendered': played.blocks,
    'blockFrames': played.blockFrames,
    'renderMeanUs': round(played.meanNs / 1000, 2),
    'renderMaxUs': round(played.maxNs / 1000, 2),
    'renderMeanShare': round(played.meanNs / played.blockNs, 5),
    'renderMaxShare': round(played.maxNs / played.blockNs, 5),
    'outputPeak': played.peak,
  };

  // Renders an impulse through a limiter offline at [sampleRate] and
  // returns the latency the graph reports and the frames the impulse took.
  ({int latency, int delay, double value}) impulseThroughLimiter(
    double sampleRate,
  ) {
    const at = 100;
    const frames = 4800;
    final graph = AudGraphFfi(
      sampleRate: sampleRate,
      maxFrames: 256,
      inputChannels: const [2],
      listen: false,
    );
    try {
      AudDspEffects.registerWith(graph.hostApi);
      final limiter = graph.createNode(AudLimiter.typeId);
      graph.transaction(
        (tx) => tx
          ..connect(graph.io, limiter)
          ..connect(limiter, graph.io),
      );
      graph.start();
      final impulse = Float32List(frames)..[at] = 0.5;
      final left = AudOfflineRenderer(graph).render(
        frames: frames,
        blockFrames: 256,
        inputs: [
          [impulse, impulse],
        ],
      )[0][0];
      final first = left.indexWhere((x) => x != 0);
      return (
        latency: graph.latencyOf(limiter),
        delay: first - at,
        value: first < 0 ? 0 : left[first],
      );
    } finally {
      graph.dispose();
    }
  }

  testWidgets('measures the render time of every node on the bench', (
    tester,
  ) async {
    await tester.pumpWidget(const EffectsApp());
    final page = tester.state<EffectsPageState>(find.byType(EffectsPage));
    final engine = page.engine;

    // The format and the block of the device while the stream runs.
    await tap(tester, 'toggle');
    expect(engine.state, AudEngineState.running);
    await tester.runAsync(
      () => Future<void>.delayed(const Duration(milliseconds: 500)),
    );
    final format = engine.format;
    final blockFrames = EffectsPageState.blockFramesOf(engine);
    await tap(tester, 'toggle');
    expect(engine.state, AudEngineState.stopped);

    // The bench runs on this thread with the engine stopped.
    final configs = [
      (sampleRate: format.sampleRate, frames: blockFrames),
      (sampleRate: 48000.0, frames: 256),
    ];
    final results = [
      for (final typeId in _nodeTypes)
        for (final config in configs)
          AudEffectsBench.measure(
            typeId,
            sampleRate: config.sampleRate,
            frames: config.frames,
          ),
    ];
    final bench = {
      'device': {...formatJson(format), 'blockFrames': blockFrames},
      'results': [
        for (final result in results)
          {
            'typeId': result.typeId,
            'sampleRate': result.sampleRate,
            'frames': result.frames,
            'blocks': result.blocks,
            'meanUs': round(result.meanNs / 1000, 2),
            'p99Us': round(result.p99Ns / 1000, 2),
            'maxUs': round(result.maxNs / 1000, 2),
            'meanShare': round(result.meanShare, 6),
            'p99Share': round(result.p99Share, 6),
            'maxShare': round(result.maxShare, 6),
          },
      ],
    };
    print('EFFECTS_BENCH ${jsonEncode(bench)}');
    for (final result in results) {
      expect(result.blocks, greaterThan(0), reason: result.typeId);
      expect(result.meanNs, greaterThan(0), reason: result.typeId);
      expect(result.maxNs, greaterThanOrEqualTo(result.p99Ns));
    }
  });

  testWidgets('plays the chord without and with delay, plate and limiter', (
    tester,
  ) async {
    await tester.pumpWidget(const EffectsApp(effects: false));
    final page = tester.state<EffectsPageState>(find.byType(EffectsPage));
    final engine = page.engine;
    final graph = engine.graph;
    final chain = page.chain;
    expect(page.error, isNull);
    expect(chain.effects, isFalse);
    expect(chain.reverb, EffectsReverb.plate);

    // The chord straight to the output; the gate opens before the measured
    // part.
    await tap(tester, 'toggle');
    expect(engine.state, AudEngineState.running);
    await tap(tester, 'chord');
    expect(chain.gate, isTrue);
    await animate(tester, const Duration(milliseconds: 500));
    final dry = await measure(tester, engine);

    // The same chord through delay -> plate -> limiter, driven 12 dB over
    // full scale; the fade of the dry connections passes first.
    page.setDrive(_driveDb);
    await tester.pump();
    expect(find.text('Drive +12.0 dB'), findsOneWidget);
    await tap(tester, 'effects');
    expect(chain.effects, isTrue);
    await animate(tester, const Duration(milliseconds: 500));
    final wet = await measure(tester, engine);
    final format = engine.format;
    final limiter = chain.limiter!;
    final latency = graph.latencyOf(limiter);
    final outputLatency = graph.outputLatency;
    expect(chain.valueOf(AudLimiter.typeId, AudLimiter.gain), _driveDb);

    // The page: Freeverb swaps in and out, the report is copied, the
    // effects go.
    await tap(tester, 'reverb-freeverb');
    expect(chain.reverb, EffectsReverb.freeverb);
    expect(chain.reverbNode!.typeId, AudFreeverb.typeId);
    expect(
      find.byKey(Key('${AudFreeverb.typeId}/${AudFreeverb.roomSize}')),
      findsOneWidget,
    );
    await tap(tester, 'reverb-plate');
    expect(chain.reverbNode!.typeId, AudPlateReverb.typeId);
    await tap(tester, 'copy');
    expect(find.text('Report copied'), findsOneWidget);
    expect(page.report(), contains('limiterLatencyFrames: $latency'));
    await tap(tester, 'effects');
    expect(chain.effects, isFalse);
    expect(chain.limiter, isNull);
    await tap(tester, 'toggle');
    expect(engine.state, AudEngineState.stopped);

    // An impulse through the limiter, offline at the rate of the engine.
    final offline = impulseThroughLimiter(format.sampleRate);

    final ceiling = pow(10, -1 / 20).toDouble();
    final measured = {
      'seconds': _seconds,
      'format': formatJson(format),
      'chord': {
        'notes': EffectsChain.notes,
        'waveform': 'saw',
        'voiceAmplitude': EffectsChain.voiceAmplitude,
      },
      'chain': [
        AudStereoDelay.typeId,
        AudPlateReverb.typeId,
        AudLimiter.typeId,
      ],
      'driveDb': _driveDb,
      'ceiling': ceiling,
      'effects': playedJson(wet),
      'dry': playedJson(dry),
      'renderMeanDiffUs': round((wet.meanNs - dry.meanNs) / 1000, 2),
      'renderMaxDiffUs': round((wet.maxNs - dry.maxNs) / 1000, 2),
      'latency': {
        'limiterFrames': latency,
        'limiterMs': round(latency / format.sampleRate * 1000, 3),
        'graphOutputLatencyFrames': outputLatency,
        'deviceOutputLatencyFrames': engine.numbers.outputLatencyFrames,
        'offline': {
          'sampleRate': format.sampleRate,
          'latencyFrames': offline.latency,
          'impulseDelayFrames': offline.delay,
          'impulseValue': offline.value,
        },
      },
    };
    print('EFFECTS_CHAIN ${jsonEncode(measured)}');

    expect(engine.counters.renderErrors, 0);
    expect(wet.callbacks, greaterThan(0));
    expect(dry.callbacks, greaterThan(0));
    // The chord sounded, driven into the limiter, and no sample passed the
    // ceiling.
    expect(wet.peak, greaterThan(0.5 * ceiling));
    expect(wet.peak, lessThanOrEqualTo(1.0));
    expect(wet.peak, lessThanOrEqualTo(ceiling + 1e-6));
    // The latency is the look-ahead, and an impulse takes exactly as long.
    expect(latency, AudLimiter.lookaheadFrames(format.sampleRate));
    expect(outputLatency, latency);
    expect(offline.latency, latency);
    expect(offline.delay, latency);
    expect(offline.value, 0.5);
  });
}

// What the engine played in a measured part: the callbacks, xruns and late
// callbacks, the blocks, their mean frames and render time, the duration of
// a block and the output peak.
typedef _Played = ({
  int callbacks,
  int xruns,
  int lateCallbacks,
  int blocks,
  int blockFrames,
  double meanNs,
  int maxNs,
  double blockNs,
  double peak,
});
