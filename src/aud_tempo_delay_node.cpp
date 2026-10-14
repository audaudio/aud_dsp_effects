// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_tempo_delay_node.hpp"

#include <cmath>

namespace aud_effects {

namespace {

// The longest delay: a whole note at 60 BPM.
constexpr double kMaxSeconds = 4.0;

}  // namespace

// ############################################################################
// AudTempoDelayNode

AudTempoDelayNode::AudTempoDelayNode(const AudNodeDescriptor*,
                                     const AudHostApi*)
    : AudDelayNode(kTempoDelayParams, AUD_TEMPO_DELAY_NUM_PARAMS,
                   AUD_TEMPO_DELAY_PARAM_FEEDBACK, AUD_TEMPO_DELAY_PARAM_MIX,
                   AUD_TEMPO_DELAY_PARAM_PING_PONG, kMaxSeconds) {}

double AudTempoDelayNode::secondsAt(double tempo) const {
  int division = stepOf(AUD_TEMPO_DELAY_PARAM_DIVISION);
  if (division < 0) division = 0;
  if (division >= AUD_TEMPO_DELAY_NUM_DIVISIONS) {
    division = AUD_TEMPO_DELAY_NUM_DIVISIONS - 1;
  }
  return kDivisionBeats[division] * 60.0 / tempo;
}

// The tempo of the segment at the start of the block: the transport fills
// it for every segment, also while it stops.
AudDelayTime AudTempoDelayNode::timeOf(const AudProcessContext& context) {
  const AudTransportSnapshot* transport = context.transport;
  double tempo = kDefaultTempo;
  if (transport != nullptr && transport->num_segments > 0 &&
      transport->segments != nullptr) {
    const double value = transport->segments[0].tempo;
    if (value > 0 && std::isfinite(value)) tempo = value;
  }
  tempo_ = tempo;
  const float seconds = static_cast<float>(secondsAt(tempo));
  return AudDelayTime{seconds, 0.0f, 0, seconds};
}

double AudTempoDelayNode::tailSeconds() const {
  const double seconds = secondsAt(tempo_);
  return seconds < maxSeconds() ? seconds : maxSeconds();
}

// ############################################################################
// The node type

namespace {

const AudNodeVTable kVTable = effectVTable<AudTempoDelayNode>();

const AudNodeDescriptor kDescriptor = effectDescriptor(
    AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID, "Tempo delay",
    kCapabilities | AUD_NODE_CAP_TRANSPORT, kStereoInputs, kStereoOutputs,
    AUD_TEMPO_DELAY_NUM_PARAMS, kTempoDelayParams, &kVTable);

}  // namespace

const AudNodeDescriptor& tempoDelayDescriptor() { return kDescriptor; }

}  // namespace aud_effects
