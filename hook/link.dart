// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:record_use/record_use.dart';

// Keeps the native library of the effects in a profile or release build only
// when the app uses it (interop-001): the members of AudDspEffects and
// AudEffectsBench, the only Dart code that calls into the library, are
// recorded with @RecordUse. Without recordings the library stays, since
// nothing says it is unused.
void main(List<String> args) async {
  await link(args, (input, output) async {
    // The recordings are a dependency: the hooks runner caches a link by
    // the recordings of the build that ran it first, so a build whose
    // recordings live elsewhere - other defines, another flavor - would
    // take the old answer.
    // ignore: deprecated_member_use
    final recordings = input.recordedUsagesFile;
    if (recordings != null) output.dependencies.add(recordings);
    if (usesEffects(input.recordedUses)) {
      output.assets.code.addAll(input.assets.code);
    }
  });
}

/// The library of `AudDspEffects`; test/hook/link_test.dart checks the
/// libraries and [effectsMembers] against the sources, so that a rename
/// cannot drop the native library of an app that uses it.
const String effectsLibrary =
    'package:aud_dsp_effects/src/aud_dsp_effects_ffi.dart';

/// The library of `AudEffectsBench`.
const String benchLibrary =
    'package:aud_dsp_effects/src/aud_effects_bench.dart';

const _effects = Class('AudDspEffects', Library(effectsLibrary));

const _bench = Class('AudEffectsBench', Library(benchLibrary));

/// The members of the package that call into its native library.
const List<DefinitionWithStaticCalls> effectsMembers = [
  Getter('register', _effects),
  Method('registerWith', _effects),
  Method('measure', _bench),
];

/// Whether an app with the recorded [uses] needs the native library.
bool usesEffects(Recordings? uses) =>
    uses == null ||
    effectsMembers.any((member) => (uses.calls[member] ?? const []).isNotEmpty);
