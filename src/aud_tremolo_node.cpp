// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_tremolo_node.hpp"

#include <algorithm>

namespace aud_effects {

// ############################################################################
// AudTremoloNode

AudTremoloNode::AudTremoloNode(const AudNodeDescriptor*, const AudHostApi*)
    : AudEffectNode(kTremoloParams, AUD_TREMOLO_NUM_PARAMS) {}

int32_t AudTremoloNode::allocate(const AudPrepareInfo&, uint32_t, uint32_t) {
  rate_ = -1.0f;
  return AUD_OK;
}

void AudTremoloNode::clear() { lfo_.restart(); }

void AudTremoloNode::renderFrames(const AudProcessContext&,
                                  const AudAudioBus& in,
                                  const AudAudioBus& out, uint32_t offset,
                                  uint32_t frames) {
  if (out.num_channels < 2) return;
  const float* inLeft = nullptr;
  const float* inRight = nullptr;
  inputPair(in, offset, inLeft, inRight);
  float* outLeft = out.channels[0] + offset;
  float* outRight = out.channels[1] + offset;
  for (uint32_t start = 0; start < frames; start += kControlFrames) {
    const uint32_t end = std::min(frames, start + kControlFrames);
    const float rate = paramAt(AUD_TREMOLO_PARAM_RATE, start);
    if (rate != rate_) {
      lfo_.setFrequency(rate, sampleRate());
      rate_ = rate;
    }
    for (uint32_t i = start; i < end; ++i) {
      const float depth = paramAt(AUD_TREMOLO_PARAM_DEPTH, i);
      const float sine = static_cast<float>(lfo_.sine());
      const float gain = 1.0f - depth * 0.5f * (1.0f - sine);
      // Both inputs are read before the outputs, which may alias them.
      const float left = inLeft[i];
      const float right = inRight[i];
      outLeft[i] = left * gain;
      outRight[i] = right * gain;
      lfo_.advance();
    }
  }
  lfo_.renormalize();
}

// ############################################################################
// The node type

namespace {

const AudNodeVTable kVTable = effectVTable<AudTremoloNode>();

const AudNodeDescriptor kDescriptor = effectDescriptor(
    AUD_DSP_EFFECTS_TREMOLO_TYPE_ID, "Tremolo", kCapabilities, kStereoInputs,
    kStereoOutputs, AUD_TREMOLO_NUM_PARAMS, kTremoloParams, &kVTable);

}  // namespace

const AudNodeDescriptor& tremoloDescriptor() { return kDescriptor; }

}  // namespace aud_effects
