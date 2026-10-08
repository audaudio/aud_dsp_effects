// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'dart:ffi';

import 'package:aud_audio_core/aud_audio_core.dart';
import 'package:aud_dsp_effects/aud_dsp_effects.dart';
import 'package:ffi/ffi.dart';
import 'package:test/test.dart';

// A fake host: records the descriptors a package registers and answers
// with a configurable result. Registration runs synchronously on the
// calling thread, so an isolate-local callable serves as the callback.
class FakeHost {
  FakeHost({this.result = AUD_OK, int abiMajor = AudAbi.major}) {
    api = calloc<AudHostApi>();
    api.ref
      ..struct_size = sizeOf<AudHostApi>()
      ..abi_major = abiMajor
      ..abi_minor = AudAbi.minor
      ..host = api.cast()
      ..register_node_type = _register.nativeFunction;
  }

  final int result;
  late final Pointer<AudHostApi> api;
  final List<AudNodeDescriptor> descriptors = [];
  late final _register =
      NativeCallable<
        Int32 Function(Pointer<Void>, Pointer<AudNodeDescriptor>)
      >.isolateLocal(_onRegister, exceptionalReturn: AUD_ERROR_FAILED);

  int _onRegister(Pointer<Void> host, Pointer<AudNodeDescriptor> descriptor) {
    expect(host, api.cast<Void>());
    descriptors.add(descriptor.ref);
    return result;
  }

  void dispose() {
    _register.close();
    calloc.free(api);
  }
}

void main() {
  group('AudTremolo', () {
    test('names the type and the parameter indices', () {
      expect(AudTremolo.typeId, 'aud.effects.tremolo');
      expect(AudTremolo.rate, 0);
      expect(AudTremolo.depth, 1);
    });
  });

  group('AudDspEffects.register(hostApi)', () {
    late FakeHost host;

    tearDown(() => host.dispose());

    test('registers the tremolo with the ABI version it was built against', () {
      host = FakeHost();
      AudDspEffects.register(host.api.cast());
      final descriptor = host.descriptors.single;
      expect(descriptor.type_id.cast<Utf8>().toDartString(), AudTremolo.typeId);
      expect(descriptor.abi_major, AudAbi.major);
      expect(descriptor.abi_minor, AudAbi.minor);
      expect(descriptor.struct_size, sizeOf<AudNodeDescriptor>());
      expect(descriptor.capabilities & AUD_NODE_CAP_IN_PLACE, isNonZero);
      expect(descriptor.num_inputs, 1);
      expect(descriptor.num_outputs, 1);
      expect(descriptor.num_params, 2);
      final params = descriptor.params;
      expect(params[0].id.cast<Utf8>().toDartString(), 'rate');
      expect(params[0].unit.cast<Utf8>().toDartString(), 'Hz');
      expect(params[1].id.cast<Utf8>().toDartString(), 'depth');
      expect(params[1].max_value, 1);
      final vtable = descriptor.vtable.ref;
      expect(vtable.struct_size, sizeOf<AudNodeVTable>());
      expect(vtable.process, isNot(nullptr));
      expect(vtable.event, nullptr);
    });

    test('throws when the host refuses the node type', () {
      host = FakeHost(result: AUD_ERROR_DUPLICATE_TYPE);
      expect(
        () => AudDspEffects.register(host.api.cast()),
        throwsA(
          isA<AudDspEffectsException>()
              .having((e) => e.code, 'code', AUD_ERROR_DUPLICATE_TYPE)
              .having((e) => e.toString(), 'toString', contains('DUPLICATE')),
        ),
      );
    });

    test('refuses a host of another ABI major without registering', () {
      host = FakeHost(abiMajor: AudAbi.major + 1);
      expect(
        () => AudDspEffects.register(host.api.cast()),
        throwsA(
          isA<AudDspEffectsException>().having(
            (e) => e.code,
            'code',
            AUD_ERROR_ABI_MAJOR,
          ),
        ),
      );
      expect(host.descriptors, isEmpty);
    });

    test('refuses a null host', () {
      host = FakeHost();
      expect(
        () => AudDspEffects.register(nullptr),
        throwsA(
          isA<AudDspEffectsException>().having(
            (e) => e.code,
            'code',
            AUD_ERROR_INVALID_ARGUMENT,
          ),
        ),
      );
    });
  });
}
