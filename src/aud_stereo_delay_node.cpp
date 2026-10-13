// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_stereo_delay_node.hpp"

#include <algorithm>
#include <cmath>

namespace aud_effects {

// ############################################################################
// AudDelayNode

AudDelayNode::AudDelayNode(const AudParamDescriptor* params,
                           uint32_t numParams, uint32_t feedback, uint32_t mix,
                           uint32_t pingPong, double maxSeconds)
    : AudEffectNode(params, numParams),
      feedback_(feedback),
      mix_(mix),
      pingPong_(pingPong),
      maxSeconds_(maxSeconds) {}

int32_t AudDelayNode::allocate(const AudPrepareInfo& info, uint32_t,
                               uint32_t) {
  const double rate = info.sample_rate;
  // Room for the longest delay and the frame the interpolation reads next
  // to it.
  const double frames = std::ceil(maxSeconds_ * rate) + 4.5;
  auto fresh = std::make_unique<DunneCore::StereoDelay>();
  fresh->init(rate, frames * 1000.0 / rate);
  fresh->setPingPongMode(pingPongOn_);
  delay_.swap(fresh);
  glide_.setTime(kDelayGlideSeconds, rate);
  return AUD_OK;
}

void AudDelayNode::clear() {
  if (delay_) delay_->clear();
  snap_ = true;
}

void AudDelayNode::renderFrames(const AudProcessContext& context,
                                const AudAudioBus& in, const AudAudioBus& out,
                                uint32_t offset, uint32_t frames) {
  if (out.num_channels < 2 || !delay_) return;
  const float* inLeft = nullptr;
  const float* inRight = nullptr;
  inputPair(in, offset, inLeft, inRight);
  float* outLeft = out.channels[0] + offset;
  float* outRight = out.channels[1] + offset;
  const double rate = sampleRate();
  const AudDelayTime time = timeOf(context);
  const auto delayAt = [&](uint32_t i) {
    const double seconds = time.at(i);
    return std::min(std::max(seconds, kMinDelaySeconds), maxSeconds_) * rate;
  };
  if (snap_) {
    glide_.reset(delayAt(0));
    snap_ = false;
  }
  const bool pingPong = targetOf(pingPong_) > 0.5f;
  if (pingPong != pingPongOn_) {
    delay_->setPingPongMode(pingPong);
    pingPongOn_ = pingPong;
  }
  for (uint32_t i = 0; i < frames; ++i) {
    // Both inputs are read before the outputs, which may alias them.
    const float dryLeft = inLeft[i];
    const float dryRight = inRight[i];
    delay_->setDelaySamples(glide_.next(delayAt(i)));
    delay_->setFeedback(paramAt(feedback_, i));
    delay_->setDryWetMix(paramAt(mix_, i));
    float left = 0.0f;
    float right = 0.0f;
    delay_->renderFrame(dryLeft, dryRight, left, right);
    outLeft[i] = flush(left);
    outRight[i] = flush(right);
  }
}

// Until the echoes have decayed by 60 dB: each period of the delay - two
// delays in ping-pong, where an echo crosses both lines - multiplies them by
// the feedback; a feedback of 0.999 and more never ends.
uint32_t AudDelayNode::computeTail() const {
  const double feedback = targetOf(feedback_);
  if (feedback >= 0.999) return AUD_TAIL_INFINITE;
  const double rate = sampleRate();
  const double seconds = std::max(tailSeconds(), glide_.value() / rate);
  const double period = (targetOf(pingPong_) > 0.5f ? 2.0 : 1.0) * seconds;
  const double repeats =
      feedback > 0 ? std::ceil(std::log(0.001) / std::log(feedback)) : 0.0;
  return tailFrames(period * (1.0 + repeats) * rate, rate);
}

// ############################################################################
// AudStereoDelayNode

AudStereoDelayNode::AudStereoDelayNode(const AudNodeDescriptor*,
                                       const AudHostApi*)
    : AudDelayNode(kStereoDelayParams, AUD_STEREO_DELAY_NUM_PARAMS,
                   AUD_STEREO_DELAY_PARAM_FEEDBACK,
                   AUD_STEREO_DELAY_PARAM_MIX,
                   AUD_STEREO_DELAY_PARAM_PING_PONG, 2.0) {}

AudDelayTime AudStereoDelayNode::timeOf(const AudProcessContext&) {
  const AudParamRamp& r = ramp(AUD_STEREO_DELAY_PARAM_TIME);
  const float value = clamped(AUD_STEREO_DELAY_PARAM_TIME, r.value());
  const float target = clamped(AUD_STEREO_DELAY_PARAM_TIME, r.target());
  // A ramp between values out of range moves within the range only.
  if (!std::isfinite(r.step()) || value != r.value() || target != r.target()) {
    return AudDelayTime{target, 0.0f, 0, target};
  }
  return AudDelayTime{value, r.step(), r.remaining(), target};
}

double AudStereoDelayNode::tailSeconds() const {
  return targetOf(AUD_STEREO_DELAY_PARAM_TIME);
}

// ############################################################################
// The node type

namespace {

const AudNodeVTable kVTable = effectVTable<AudStereoDelayNode>();

const AudNodeDescriptor kDescriptor = effectDescriptor(
    AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID, "Stereo delay", kCapabilities,
    kStereoInputs, kStereoOutputs, AUD_STEREO_DELAY_NUM_PARAMS,
    kStereoDelayParams, &kVTable);

}  // namespace

const AudNodeDescriptor& stereoDelayDescriptor() { return kDescriptor; }

}  // namespace aud_effects
