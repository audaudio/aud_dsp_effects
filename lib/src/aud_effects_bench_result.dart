// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// #############################################################################
/// The render time of a node as `AudEffectsBench.measure` timed it: the
/// mean, the 99th percentile and the maximum of the process calls of its
/// blocks, in nanoseconds and as shares of the duration of a block.
class AudEffectsBenchResult {
  /// Creates a result.
  const AudEffectsBenchResult({
    required this.typeId,
    required this.sampleRate,
    required this.frames,
    required this.channels,
    required this.blocks,
    required this.blockNs,
    required this.meanNs,
    required this.p99Ns,
    required this.maxNs,
  });

  // ...........................................................................
  /// The result as JSON, e.g. for the report of an integration test.
  Map<String, Object> toJson() => {
    'typeId': typeId,
    'sampleRate': sampleRate,
    'frames': frames,
    'channels': channels,
    'blocks': blocks,
    'blockNs': blockNs,
    'meanNs': meanNs,
    'p99Ns': p99Ns,
    'maxNs': maxNs,
    'meanShare': meanShare,
    'p99Share': p99Share,
    'maxShare': maxShare,
  };

  @override
  String toString() =>
      '$typeId: mean ${_micros(meanNs)} µs, p99 ${_micros(p99Ns)} µs, '
      'max ${_micros(maxNs)} µs per block of $frames frames at '
      '${sampleRate.round()} Hz (${(meanShare * 100).toStringAsFixed(2)} % '
      'of the block)';

  // ...........................................................................
  /// The type id of the node.
  final String typeId;

  /// The sample rate in Hz.
  final double sampleRate;

  /// The frames of a block.
  final int frames;

  /// The channels of the node's input.
  final int channels;

  /// The blocks that were timed.
  final int blocks;

  /// The duration of a block at [sampleRate] in nanoseconds.
  final double blockNs;

  /// The mean time of a block's process call in nanoseconds.
  final double meanNs;

  /// The 99th percentile of the process calls in nanoseconds.
  final double p99Ns;

  /// The longest process call in nanoseconds.
  final double maxNs;

  // ...........................................................................
  /// [meanNs] as a share of [blockNs]: 0.01 takes 1 % of the real time.
  double get meanShare => meanNs / blockNs;

  /// [p99Ns] as a share of [blockNs].
  double get p99Share => p99Ns / blockNs;

  /// [maxNs] as a share of [blockNs].
  double get maxShare => maxNs / blockNs;

  static String _micros(double ns) => (ns / 1000).toStringAsFixed(1);
}
