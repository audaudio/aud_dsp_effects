// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:math';

import 'package:aud_audio/aud_audio.dart';
import 'package:aud_dsp_effects/aud_dsp_effects.dart';

// #############################################################################
/// The reverb of an [EffectsChain]: the plate, the master reverb of the app,
/// or Freeverb.
enum EffectsReverb {
  /// The plate reverb of Dattorro, `aud.effects.plate`.
  plate(AudPlateReverb.typeId, 'Plate', AudPlateReverb.paramDescriptors),

  /// Jezar's Freeverb, `aud.effects.freeverb`.
  freeverb(AudFreeverb.typeId, 'Freeverb', AudFreeverb.paramDescriptors);

  /// Creates a reverb of the node type [typeId].
  const EffectsReverb(this.typeId, this.label, this.params);

  /// The type id of the node.
  final String typeId;

  /// The name on the page.
  final String label;

  /// The parameters of the node.
  final List<AudParamDescriptor> params;
}

// #############################################################################
/// A dense chord through the master chain of an app on a graph: eight saw
/// oscillators across three octaves, summed into one input, then the stereo
/// delay, a reverb and the limiter before the output.
///
/// The parameters of the effects start at the defaults of the nodes and
/// survive when the effects are switched off or the reverb is swapped: the
/// chain sets them on every node it creates.
class EffectsChain {
  /// Creates the oscillators and - with [effects] - the delay, the plate and
  /// the limiter on [graph], whose packages have to be registered.
  EffectsChain({required this.graph, bool effects = true}) {
    for (final note in notes) {
      final oscillator = graph.createNode(AUD_GRAPH_OSCILLATOR_TYPE_ID);
      graph
        ..setParam(oscillator, AUD_OSCILLATOR_PARAM_WAVEFORM, sawWaveform)
        ..setParam(oscillator, AUD_OSCILLATOR_PARAM_FREQUENCY, frequency(note))
        ..setParam(oscillator, AUD_OSCILLATOR_PARAM_AMPLITUDE, 0);
      oscillators.add(oscillator);
    }
    if (effects) {
      _insertEffects(dry: false);
    } else {
      graph.transaction((tx) {
        for (final oscillator in oscillators) {
          tx.connect(oscillator, graph.io);
        }
      });
    }
  }

  // ...........................................................................
  /// The value of the parameter [param] of the node type [typeId].
  double valueOf(String typeId, String param) => _values[typeId]![param]!;

  /// Opens the chord ([on]) or closes it, ramped.
  void setGate(bool on) {
    _gate = on;
    for (final oscillator in oscillators) {
      graph.setParam(
        oscillator,
        AUD_OSCILLATOR_PARAM_AMPLITUDE,
        on ? voiceAmplitude : 0,
        rampFrames: rampFrames,
      );
    }
  }

  /// Routes the chord through the effects ([on]) or straight to the
  /// output; the effects are removed from the graph while they are off, so
  /// that they cost no render time.
  void setEffects(bool on) {
    if (on == _effects) return;
    if (on) {
      _insertEffects(dry: true);
      return;
    }
    graph.transaction((tx) {
      for (final node in [_delay!, _reverbNode!, _limiter!]) {
        tx.remove(node);
      }
      for (final oscillator in oscillators) {
        tx.connect(oscillator, graph.io);
      }
    });
    _delay = _reverbNode = _limiter = null;
    _effects = false;
  }

  /// Swaps the reverb for [reverb]; the old one rings out.
  void selectReverb(EffectsReverb reverb) {
    if (reverb == _reverb) return;
    _reverb = reverb;
    if (!_effects) return;
    final next = _create(reverb.typeId);
    graph.transaction(
      (tx) => tx
        ..remove(_reverbNode!)
        ..connect(_delay!, next)
        ..connect(next, _limiter!),
    );
    _reverbNode = next;
  }

  /// Sets the parameter [param] of the node type [typeId] to [value],
  /// ramped when the parameter ramps.
  void setParam(String typeId, String param, double value) {
    _values[typeId]![param] = value;
    final node = _nodeOf(typeId);
    if (node == null) return;
    final ramped = _descriptors[typeId]!.any((p) => p.id == param && p.ramped);
    graph.setParam(node, param, value, rampFrames: ramped ? rampFrames : 0);
  }

  /// Sets the input gain of the limiter to [db].
  void setDrive(double db) => setParam(AudLimiter.typeId, AudLimiter.gain, db);

  /// Removes the nodes of the chain from the graph.
  void dispose() => graph.transaction((tx) {
    for (final node in [...oscillators, ?_delay, ?_reverbNode, ?_limiter]) {
      tx.remove(node);
    }
  });

  /// The route and the parameters as JSON, for the report.
  Map<String, Object?> toJson() => {
    'gate': _gate,
    'effects': _effects,
    'chain': _effects
        ? [AudStereoDelay.typeId, _reverb.typeId, AudLimiter.typeId]
        : const <String>[],
    'notes': notes,
    'voiceAmplitude': voiceAmplitude,
    'params': {
      for (final typeId in [
        AudStereoDelay.typeId,
        _reverb.typeId,
        AudLimiter.typeId,
      ])
        typeId: Map<String, double>.of(_values[typeId]!),
    },
  };

  // ...........................................................................
  /// The graph.
  final AudGraph graph;

  /// The oscillators of the chord, one per note of [notes].
  final List<AudNode> oscillators = [];

  /// Whether the chord sounds.
  bool get gate => _gate;

  /// Whether the chord plays through the effects; without them it goes
  /// straight to the output.
  bool get effects => _effects;

  /// The reverb of the chain.
  EffectsReverb get reverb => _reverb;

  /// The stereo delay, or null without the effects.
  AudNode? get delay => _delay;

  /// The reverb node, or null without the effects.
  AudNode? get reverbNode => _reverbNode;

  /// The limiter, or null without the effects.
  AudNode? get limiter => _limiter;

  /// The input gain of the limiter in dB.
  double get drive => valueOf(AudLimiter.typeId, AudLimiter.gain);

  // ...........................................................................
  /// The MIDI notes of the chord across three octaves: C2, G2, E3, G3, B3,
  /// D4, E4, B4.
  static const List<int> notes = [36, 43, 52, 55, 59, 62, 64, 71];

  /// The amplitude of each voice: the sum of the eight saws peaks at full
  /// scale at most.
  static const double voiceAmplitude = 0.125;

  /// The waveform parameter of the oscillator for a saw.
  static const double sawWaveform = 1;

  /// The frames over which the gate and the parameters move.
  static const int rampFrames = 480;

  /// The frequency of the MIDI [note] in Hz.
  static double frequency(int note) =>
      440 * pow(2, (note - 69) / 12).toDouble();

  /// The parameters of the delay, both reverbs and the limiter, by type id.
  static final Map<String, List<AudParamDescriptor>> _descriptors = {
    AudStereoDelay.typeId: AudStereoDelay.paramDescriptors,
    for (final reverb in EffectsReverb.values) reverb.typeId: reverb.params,
    AudLimiter.typeId: AudLimiter.paramDescriptors,
  };

  // ######################
  // Private
  // ######################

  final Map<String, Map<String, double>> _values = {
    for (final MapEntry(:key, :value) in _descriptors.entries)
      key: {for (final param in value) param.id: param.defaultValue},
  };
  bool _gate = false;
  bool _effects = false;
  EffectsReverb _reverb = EffectsReverb.plate;
  AudNode? _delay;
  AudNode? _reverbNode;
  AudNode? _limiter;

  // Creates the delay, the reverb and the limiter and routes the chord
  // through them; [dry] the chord went straight to the output so far.
  void _insertEffects({required bool dry}) {
    final delay = _create(AudStereoDelay.typeId);
    final reverb = _create(_reverb.typeId);
    final limiter = _create(AudLimiter.typeId);
    graph.transaction((tx) {
      for (final oscillator in oscillators) {
        if (dry) tx.disconnect(oscillator, graph.io);
        tx.connect(oscillator, delay);
      }
      tx
        ..connect(delay, reverb)
        ..connect(reverb, limiter)
        ..connect(limiter, graph.io);
    });
    _delay = delay;
    _reverbNode = reverb;
    _limiter = limiter;
    _effects = true;
  }

  // Creates a node of [typeId] with the parameter values of the chain.
  AudNode _create(String typeId) {
    final node = graph.createNode(typeId);
    for (final MapEntry(:key, :value) in _values[typeId]!.entries) {
      graph.setParam(node, key, value);
    }
    return node;
  }

  // The node of [typeId] in the chain, or null.
  AudNode? _nodeOf(String typeId) => switch (typeId) {
    AudStereoDelay.typeId => _delay,
    AudLimiter.typeId => _limiter,
    _ when typeId == _reverb.typeId => _reverbNode,
    _ => null,
  };
}
