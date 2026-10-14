// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

import 'package:aud_audio/aud_audio.dart';
import 'package:flutter/material.dart';

// #############################################################################
/// The control of one parameter as its node declares it: a switch for a
/// boolean, a slider with steps for a stepped one, a slider otherwise; the
/// name, the value and the unit above it.
class ParamSlider extends StatelessWidget {
  /// Creates the control of [param] at [value].
  ///
  /// - [onChanged] receives the new value
  /// - [label] replaces the name of the parameter
  /// - [min], [max] replace the range of the parameter
  const ParamSlider({
    super.key,
    required this.param,
    required this.value,
    required this.onChanged,
    this.label,
    this.min,
    this.max,
  });

  // ...........................................................................
  /// The parameter.
  final AudParamDescriptor param;

  /// The value.
  final double value;

  /// Receives the new value.
  final ValueChanged<double> onChanged;

  /// The name above the control; the parameter's name by default.
  final String? label;

  /// The lower end of the slider; the parameter's by default.
  final double? min;

  /// The upper end of the slider; the parameter's by default.
  final double? max;

  // ...........................................................................
  @override
  Widget build(BuildContext context) {
    final name = label ?? param.name;
    if (param.boolean) {
      return SwitchListTile(
        contentPadding: EdgeInsets.zero,
        title: Text(name),
        value: value >= 0.5,
        onChanged: (on) => onChanged(on ? 1 : 0),
      );
    }
    final low = min ?? param.min;
    final high = max ?? param.max;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text('$name ${format(param, value)}'),
        Slider(
          value: value.clamp(low, high),
          min: low,
          max: high,
          divisions: param.stepped && param.steps > 1 ? param.steps - 1 : null,
          onChanged: onChanged,
        ),
      ],
    );
  }

  /// [value] of [param] with its unit: a signed dB value, whole steps,
  /// three decimals for values up to 2, one decimal above.
  static String format(AudParamDescriptor param, double value) {
    final unit = param.unit.isEmpty ? '' : ' ${param.unit}';
    if (param.stepped) return '${value.round()}$unit';
    if (param.unit == 'dB') {
      return '${value >= 0 ? '+' : ''}${value.toStringAsFixed(1)}$unit';
    }
    final digits = param.max - param.min > 2 ? 1 : 3;
    return '${value.toStringAsFixed(digits)}$unit';
  }
}
