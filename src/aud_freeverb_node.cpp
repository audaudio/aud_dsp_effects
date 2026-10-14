// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_freeverb_node.hpp"

#include <algorithm>
#include <cmath>

namespace aud_effects {

// ############################################################################
// AudFreeverbNode

AudFreeverbNode::AudFreeverbNode(const AudNodeDescriptor*, const AudHostApi*)
    : AudEffectNode(kFreeverbParams, AUD_FREEVERB_NUM_PARAMS) {
  // The wet signal at Freeverb's own level, its default wet of 1/scalewet;
  // the node crossfades the dry signal itself.
  model_.setwet(1.0f / scalewet);
  model_.setdry(0.0f);
  applyParams(0);
}

int32_t AudFreeverbNode::allocate(const AudPrepareInfo& info, uint32_t,
                                  uint32_t) {
  std::vector<float> memory(
      static_cast<size_t>(revmodel::buffersize(info.sample_rate)), 0.0f);
  memory_.swap(memory);
  model_.setbuffers(memory_.data(), info.sample_rate);
  return AUD_OK;
}

void AudFreeverbNode::clear() { model_.mute(); }

void AudFreeverbNode::applyParams(uint32_t i) {
  const float room = paramAt(AUD_FREEVERB_PARAM_ROOM_SIZE, i);
  const float damping = paramAt(AUD_FREEVERB_PARAM_DAMPING, i);
  const float width = paramAt(AUD_FREEVERB_PARAM_WIDTH, i);
  if (room != room_) model_.setroomsize(room_ = room);
  if (damping != damping_) model_.setdamp(damping_ = damping);
  if (width != width_) model_.setwidth(width_ = width);
}

void AudFreeverbNode::renderFrames(const AudProcessContext&,
                                   const AudAudioBus& in,
                                   const AudAudioBus& out, uint32_t offset,
                                   uint32_t frames) {
  if (out.num_channels < 2 || memory_.empty()) return;
  const float* inLeft = nullptr;
  const float* inRight = nullptr;
  inputPair(in, offset, inLeft, inRight);
  float* outLeft = out.channels[0] + offset;
  float* outRight = out.channels[1] + offset;
  for (uint32_t start = 0; start < frames; start += kControlFrames) {
    const uint32_t end = std::min(frames, start + kControlFrames);
    applyParams(start);
    for (uint32_t i = start; i < end; ++i) {
      // Both inputs are read before the outputs, which may alias them.
      const float dryLeft = inLeft[i];
      const float dryRight = inRight[i];
      float wetLeft = 0.0f;
      float wetRight = 0.0f;
      model_.processframe(dryLeft, dryRight, wetLeft, wetRight);
      const float mix = paramAt(AUD_FREEVERB_PARAM_MIX, i);
      outLeft[i] = flush((1.0f - mix) * dryLeft + mix * wetLeft);
      outRight[i] = flush((1.0f - mix) * dryRight + mix * wetRight);
    }
  }
}

// Until the longest comb has decayed by 60 dB at the target room size, and
// the allpasses after it: its feedback is the gain of one pass at DC, where
// the damping passes everything.
uint32_t AudFreeverbNode::computeTail() const {
  const double rate = sampleRate();
  const double feedback =
      targetOf(AUD_FREEVERB_PARAM_ROOM_SIZE) * scaleroom + offsetroom;
  const double passes = std::log(0.001) / std::log(feedback);
  const double longest = revmodel::scaledtuning(combtuningR8, rate);
  const double allpasses = revmodel::scaledtuning(allpasstuningR1, rate) +
                           revmodel::scaledtuning(allpasstuningR2, rate) +
                           revmodel::scaledtuning(allpasstuningR3, rate) +
                           revmodel::scaledtuning(allpasstuningR4, rate);
  return tailFrames(passes * longest + allpasses, rate);
}

// ############################################################################
// The node type

namespace {

const AudNodeVTable kVTable = effectVTable<AudFreeverbNode>();

const AudNodeDescriptor kDescriptor = effectDescriptor(
    AUD_DSP_EFFECTS_FREEVERB_TYPE_ID, "Freeverb", kCapabilities, kStereoInputs,
    kStereoOutputs, AUD_FREEVERB_NUM_PARAMS, kFreeverbParams, &kVTable);

}  // namespace

const AudNodeDescriptor& freeverbDescriptor() { return kDescriptor; }

}  // namespace aud_effects
