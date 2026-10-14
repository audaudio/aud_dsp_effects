// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:io';
import 'dart:isolate';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:logging/logging.dart';
import 'package:native_toolchain_c/native_toolchain_c.dart';

// Builds the effect nodes (ticket 28) as C++17 with the vendored Freeverb
// and DunneAudioKit delay against the header-only ABI of aud_audio_core,
// resolved through the package config; the package never links the engine
// (build-001). The flags follow aud_audio_io: hidden symbols - only
// aud_dsp_effects_register and the bench are visible -, on Android 16 KB
// pages, the static C++ runtime kept inside the library, unused code
// dropped and no undefined symbol left. Floating-point contraction is off,
// so that arm64 and x64 render the same samples and the goldens hold on
// both.
void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    final packageName = input.packageName;
    final android = input.config.code.targetOS == OS.android;
    final cbuilder = CBuilder.library(
      name: packageName,
      assetName: 'src/${packageName}_bindings_generated.dart',
      sources: sources,
      includes: [
        'src',
        'src/third_party/freeverb',
        'src/third_party/dunne_audio_kit',
        await packageSrcDirectory('aud_audio_core'),
      ],
      language: Language.cpp,
      std: 'c++17',
      cppLinkStdLib: android ? 'c++_static' : null,
      flags: [
        '-fvisibility=hidden',
        '-fvisibility-inlines-hidden',
        '-ffp-contract=off',
        if (android) ...[
          '-Wl,-z,max-page-size=16384',
          // The static C++ runtime stays inside the library.
          '-Wl,--exclude-libs,ALL',
          // Code nothing calls leaves the library.
          '-ffunction-sections',
          '-fdata-sections',
          '-Wl,--gc-sections',
          // A symbol the library leaves undefined fails the link, not the
          // load in the app.
          '-Wl,--no-undefined',
        ],
      ],
      libraries: [if (android) 'm'],
    );
    await cbuilder.run(
      input: input,
      output: output,
      // In profile and release builds the link hook decides whether the
      // app uses the effects (interop-001).
      routing: [
        if (input.config.linkingEnabled)
          ToLinkHook(packageName)
        else
          const ToAppBundle(),
      ],
      logger: Logger('')
        ..level = Level.ALL
        ..onRecord.listen((record) => stdout.writeln(record.message)),
    );
  });
}

/// The C++ sources of the library, relative to the package root.
const List<String> sources = [
  'src/aud_dsp_effects.cpp',
  'src/aud_effects_bench.cpp',
  'src/aud_freeverb_node.cpp',
  'src/aud_limiter_node.cpp',
  'src/aud_plate_node.cpp',
  'src/aud_stereo_delay_node.cpp',
  'src/aud_tempo_delay_node.cpp',
  'src/aud_tremolo_node.cpp',
  'src/third_party/dunne_audio_kit/AdjustableDelayLine.cpp',
  'src/third_party/dunne_audio_kit/StereoDelay.cpp',
  'src/third_party/freeverb/allpass.cpp',
  'src/third_party/freeverb/comb.cpp',
  'src/third_party/freeverb/revmodel.cpp',
];

/// The `src` directory of [package], resolved through the package config.
Future<String> packageSrcDirectory(String package) async {
  final lib = await Isolate.resolvePackageUri(Uri.parse('package:$package/'));
  if (lib == null) throw StateError('Package $package is not resolvable');
  return lib.resolve('../src/').toFilePath();
}
