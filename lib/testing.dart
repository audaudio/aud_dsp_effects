// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

/// The bench of the effect nodes (ticket 28): the render time of one node,
/// timed through its vtable on the calling thread, in `dart test` and in
/// integration tests on phones.
library;

export 'src/aud_effects_bench.dart';
export 'src/aud_effects_bench_result.dart';
