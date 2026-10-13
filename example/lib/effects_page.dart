// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:aud_audio/aud_audio.dart';
import 'package:aud_dsp_effects/aud_dsp_effects.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'effects_chain.dart';
import 'param_slider.dart';

// #############################################################################
/// The page of the example: a dense chord through the stereo delay, a
/// reverb and the limiter on [engine], the parameters of the nodes, a drive
/// into the limiter and the numbers of the engine with a report to copy.
class EffectsPage extends StatefulWidget {
  /// Creates the page on [engine], which registers `aud_dsp_effects`;
  /// [effects] false starts the chord without the effects.
  const EffectsPage({super.key, required this.engine, this.effects = true});

  /// The engine.
  final AudEngine engine;

  /// Whether the chord starts with the effects.
  final bool effects;

  @override
  State<EffectsPage> createState() => EffectsPageState();
}

// #############################################################################
/// The state of [EffectsPage]; public for the integration test.
class EffectsPageState extends State<EffectsPage> {
  /// The engine.
  AudEngine get engine => widget.engine;

  /// The chord and the effects.
  EffectsChain get chain => _chain!;

  /// Why the chain could not be built, or null.
  String? get error => _error;

  @override
  void initState() {
    super.initState();
    try {
      // The packages register when the engine prepares for the first time.
      if (engine.registrations.isEmpty) engine.prepare();
      _chain = EffectsChain(graph: engine.graph, effects: widget.effects);
    } on Object catch (e) {
      _error = '$e';
    }
    _states = engine.states.listen((_) => _rebuild());
    _refresh = Timer.periodic(_refreshInterval, (_) => _rebuild());
  }

  @override
  void dispose() {
    _refresh.cancel();
    unawaited(_states.cancel());
    if (!engine.graph.isDisposed) _chain?.dispose();
    super.dispose();
  }

  // ...........................................................................
  /// Starts the engine when it is stopped, stops it otherwise.
  void toggleEngine() {
    if (isRunning) {
      engine.stop();
    } else {
      engine.start();
    }
    _rebuild();
  }

  /// Opens or closes the chord.
  void setGate(bool on) => setState(() => chain.setGate(on));

  /// Routes the chord through the effects or straight to the output.
  void setEffects(bool on) => setState(() => chain.setEffects(on));

  /// Swaps the reverb.
  void selectReverb(EffectsReverb reverb) =>
      setState(() => chain.selectReverb(reverb));

  /// Sets a parameter of the node type [typeId].
  void setParam(String typeId, String param, double value) =>
      setState(() => chain.setParam(typeId, param, value));

  /// Sets the input gain of the limiter in dB.
  void setDrive(double db) => setState(() => chain.setDrive(db));

  /// Zeroes the statistics of the graph: the render time and the peak.
  void resetStats() => setState(engine.graph.resetStats);

  /// Copies [report] to the clipboard.
  Future<void> copyReport() async {
    await Clipboard.setData(ClipboardData(text: report()));
    if (!mounted) return;
    ScaffoldMessenger.of(context)
        .showSnackBar(const SnackBar(content: Text('Report copied')));
  }

  /// The report of the engine with the chain, its parameters, the output
  /// peak and the latencies.
  String report() {
    final graph = engine.graph;
    final limiter = _chain?.limiter;
    final buffer = StringBuffer(engine.report())
      ..writeln('aud_dsp_effects $audDspEffectsVersion')
      ..writeln('outputPeak: ${engine.stats.outputPeak}')
      ..writeln(
        'limiterLatencyFrames: '
        '${limiter == null ? '-' : graph.latencyOf(limiter)}',
      )
      ..writeln('graphOutputLatencyFrames: ${graph.outputLatency}');
    if (_chain case final chain?) {
      buffer.writeln('chain: ${jsonEncode(chain.toJson())}');
    }
    if (_error case final error?) buffer.writeln('error: $error');
    return buffer.toString();
  }

  /// Whether the engine runs: it plays, or waits suspended for its device.
  bool get isRunning =>
      engine.state == AudEngineState.running ||
      engine.state == AudEngineState.suspended;

  /// The frames of the blocks [engine] renders: their mean since the
  /// statistics were reset - a device may call with other blocks than the
  /// buffer it reports -, else the buffer, else the largest block.
  static int blockFramesOf(AudEngine engine) {
    final stats = engine.stats;
    if (stats.blocksRendered > 0) {
      return (stats.framesRendered / stats.blocksRendered).round();
    }
    final format = engine.format;
    return format.bufferFrames > 0 ? format.bufferFrames : format.maxFrames;
  }

  // ...........................................................................
  @override
  Widget build(BuildContext context) {
    final chain = _chain;
    // A column, not a lazy list: every control exists, also off screen.
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          ..._engineHeader(context),
          if (_error != null) Text(_error!, key: const Key('error')),
          if (chain != null) ...[
            const Divider(),
            ..._chord(chain),
            const Divider(),
            ..._delay(chain),
            const Divider(),
            ..._reverb(chain),
            const Divider(),
            ..._limiter(chain),
          ],
          const Divider(),
          ..._numbers(),
        ],
      ),
    );
  }

  // ######################
  // Private
  // ######################

  // How often the page shows the numbers.
  static const Duration _refreshInterval = Duration(milliseconds: 500);

  // The largest drive into the limiter in dB.
  static const double _maxDriveDb = 24;

  EffectsChain? _chain;
  String? _error;
  late final StreamSubscription<AudEngineState> _states;
  late final Timer _refresh;

  void _rebuild() {
    if (mounted) setState(() {});
  }

  List<Widget> _engineHeader(BuildContext context) {
    final format = engine.format;
    return [
      Text(
        'State: ${engine.state.name}',
        key: const Key('state'),
        style: Theme.of(context).textTheme.titleMedium,
      ),
      Text('Route: ${engine.route.name}'),
      Text(
        'Format: ${format.sampleRate.toStringAsFixed(0)} Hz, '
        '${format.outputChannels} out, buffer ${format.bufferFrames} '
        '(${format.backend})',
      ),
      const SizedBox(height: 8),
      FilledButton(
        key: const Key('toggle'),
        onPressed: toggleEngine,
        child: Text(isRunning ? 'Stop' : 'Start'),
      ),
    ];
  }

  List<Widget> _chord(EffectsChain chain) => [
    SwitchListTile(
      key: const Key('chord'),
      contentPadding: EdgeInsets.zero,
      title: const Text('Chord'),
      subtitle: Text(
        '${EffectsChain.notes.length} saws, C2 to B4, '
        '${EffectsChain.voiceAmplitude} each',
      ),
      value: chain.gate,
      onChanged: setGate,
    ),
    SwitchListTile(
      key: const Key('effects'),
      contentPadding: EdgeInsets.zero,
      title: const Text('Effects'),
      subtitle: Text(
        chain.effects
            ? 'Delay -> ${chain.reverb.label} -> limiter -> output'
            : 'Chord -> output',
      ),
      value: chain.effects,
      onChanged: setEffects,
    ),
  ];

  List<Widget> _delay(EffectsChain chain) => [
    _title('Stereo delay', AudStereoDelay.typeId),
    ..._sliders(chain, AudStereoDelay.typeId, AudStereoDelay.paramDescriptors),
  ];

  List<Widget> _reverb(EffectsChain chain) => [
    _title('Reverb', chain.reverb.typeId),
    SegmentedButton<EffectsReverb>(
      segments: [
        for (final reverb in EffectsReverb.values)
          ButtonSegment(
            value: reverb,
            label: Text(reverb.label, key: Key('reverb-${reverb.name}')),
          ),
      ],
      selected: {chain.reverb},
      onSelectionChanged: (selected) => selectReverb(selected.single),
    ),
    ..._sliders(chain, chain.reverb.typeId, chain.reverb.params),
  ];

  List<Widget> _limiter(EffectsChain chain) => [
    _title('Limiter', AudLimiter.typeId),
    ParamSlider(
      key: const Key('drive'),
      param: AudLimiter.paramDescriptors.firstWhere(
        (param) => param.id == AudLimiter.gain,
      ),
      label: 'Drive',
      min: 0,
      max: _maxDriveDb,
      value: chain.drive,
      onChanged: setDrive,
    ),
    ..._sliders(chain, AudLimiter.typeId, [
      for (final param in AudLimiter.paramDescriptors)
        if (param.id != AudLimiter.gain) param,
    ]),
  ];

  Widget _title(String title, String typeId) => Padding(
    padding: const EdgeInsets.only(bottom: 8),
    child: Text(
      '$title ($typeId)',
      style: Theme.of(context).textTheme.titleMedium,
    ),
  );

  List<Widget> _sliders(
    EffectsChain chain,
    String typeId,
    List<AudParamDescriptor> params,
  ) => [
    for (final param in params)
      ParamSlider(
        key: Key('$typeId/${param.id}'),
        param: param,
        value: chain.valueOf(typeId, param.id),
        onChanged: (value) => setParam(typeId, param.id, value),
      ),
  ];

  List<Widget> _numbers() {
    final numbers = engine.numbers;
    final format = engine.format;
    final graph = engine.graph;
    final blockFrames = blockFramesOf(engine);
    final blockNs = format.sampleRate > 0
        ? blockFrames / format.sampleRate * 1e9
        : 0.0;
    String share(num ns) =>
        blockNs > 0 ? '${(100 * ns / blockNs).toStringAsFixed(1)} %' : '-';
    final peak = engine.stats.outputPeak;
    final limiter = _chain?.limiter;
    final latency = limiter == null ? null : graph.latencyOf(limiter);
    return [
      Text(
        'Render: mean ${_ms(numbers.renderTimeMeanNs)} ms '
        '(${share(numbers.renderTimeMeanNs)}), '
        'max ${_ms(numbers.renderTimeMaxNs)} ms '
        '(${share(numbers.renderTimeMaxNs)} of $blockFrames frames)',
        key: const Key('render'),
      ),
      Text(
        'Callbacks: ${numbers.callbacks}, xruns ${numbers.xruns}, '
        'late ${numbers.lateCallbacks}',
        key: const Key('callbacks'),
      ),
      Text(
        'Output peak: ${peak.toStringAsFixed(4)} '
        '(${peak > 0 ? (20 * log(peak) / ln10).toStringAsFixed(2) : '-inf'} '
        'dBFS)',
        key: const Key('peak'),
      ),
      Text(
        latency == null
            ? 'Limiter latency: - (effects off)'
            : 'Limiter latency: $latency frames '
                  '(${_ms(latency / format.sampleRate * 1e9)} ms), '
                  'graph output latency ${graph.outputLatency} frames',
        key: const Key('latency'),
      ),
      const SizedBox(height: 8),
      Row(
        children: [
          OutlinedButton(
            key: const Key('reset'),
            onPressed: resetStats,
            child: const Text('Reset numbers'),
          ),
          const SizedBox(width: 8),
          OutlinedButton(
            key: const Key('copy'),
            onPressed: copyReport,
            child: const Text('Copy report'),
          ),
        ],
      ),
    ];
  }

  // The nanoseconds [ns] in milliseconds with two decimals.
  static String _ms(num ns) => (ns / 1e6).toStringAsFixed(2);
}
