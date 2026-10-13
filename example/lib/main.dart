// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio/aud_audio_ffi.dart';
import 'package:aud_dsp_effects/aud_dsp_effects_ffi.dart';
import 'package:flutter/material.dart';

import 'effects_page.dart';

void main() => runApp(const EffectsApp());

// #############################################################################
/// The example of `aud_dsp_effects` on the engine of `aud_audio`: a chord
/// through the stereo delay, a reverb and the limiter, with the numbers of
/// the engine.
class EffectsApp extends StatefulWidget {
  /// Creates the app; [config] replaces the engine's configuration and
  /// [effects] false starts the chord without the effects (tests).
  const EffectsApp({
    super.key,
    this.config = const AudEngineConfig(),
    this.effects = true,
  });

  /// The configuration of the engine.
  final AudEngineConfig config;

  /// Whether the chord starts with the effects.
  final bool effects;

  @override
  State<EffectsApp> createState() => EffectsAppState();
}

// #############################################################################
/// The state of [EffectsApp]: the engine with the effects registered.
class EffectsAppState extends State<EffectsApp> {
  /// The engine.
  late final AudEngine engine = AudEngine(
    config: widget.config,
    packages: [
      AudNativeNodePackage(AudDspEffects.name, AudDspEffects.register),
    ],
  );

  @override
  void dispose() {
    engine.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'aud_dsp_effects',
    theme: ThemeData(colorSchemeSeed: Colors.teal, useMaterial3: true),
    home: Scaffold(
      appBar: AppBar(title: const Text('aud_dsp_effects')),
      body: EffectsPage(engine: engine, effects: widget.effects),
    ),
  );
}
