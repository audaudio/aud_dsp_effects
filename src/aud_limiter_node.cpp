// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_limiter_node.hpp"

#include <algorithm>
#include <cmath>

namespace aud_effects {

uint32_t limiterLookahead(double sampleRate) {
  const double frames = std::floor(kLookaheadSeconds * sampleRate + 0.5);
  return frames < 1 ? 1 : static_cast<uint32_t>(frames);
}

// ############################################################################
// AudLimiterNode

float AudLimiterNode::Decibels::of(float value) {
  if (value != db) {
    db = value;
    linear = static_cast<float>(std::pow(10.0, value / 20.0));
  }
  return linear;
}

AudLimiterNode::AudLimiterNode(const AudNodeDescriptor*, const AudHostApi*)
    : AudEffectNode(kLimiterParams, AUD_LIMITER_NUM_PARAMS) {}

int32_t AudLimiterNode::allocate(const AudPrepareInfo& info, uint32_t,
                                 uint32_t outputs) {
  const uint32_t lookahead = limiterLookahead(info.sample_rate);
  const uint32_t window = lookahead + 1;
  const uint32_t channels = std::min(outputs, kLimiterMaxChannels);
  std::vector<float> delay(static_cast<size_t>(window) * channels, 0.0f);
  std::vector<float> dequeValues(window, 1.0f);
  std::vector<uint64_t> dequeFrames(window, 0);
  std::vector<float> box(window, 1.0f);
  delay_.swap(delay);
  dequeValues_.swap(dequeValues);
  dequeFrames_.swap(dequeFrames);
  box_.swap(box);
  lookahead_ = lookahead;
  window_ = window;
  channels_ = channels;
  return AUD_OK;
}

void AudLimiterNode::clear() {
  std::fill(delay_.begin(), delay_.end(), 0.0f);
  std::fill(box_.begin(), box_.end(), 1.0f);
  delayPosition_ = 0;
  dequeFront_ = 0;
  dequeSize_ = 0;
  frame_ = 0;
  follower_ = 1.0f;
  boxPosition_ = 0;
  boxSum_ = window_;
}

float AudLimiterNode::slidingMinimum(float required) {
  // Drops the value at the front that leaves the window - at most one per
  // frame -, then the values at the back that the new one undercuts; the
  // deque then holds at most window_ - 1 values before the push.
  if (dequeSize_ > 0 && frame_ - dequeFrames_[dequeFront_] >= window_) {
    dequeFront_ = dequeFront_ + 1 == window_ ? 0 : dequeFront_ + 1;
    dequeSize_ -= 1;
  }
  while (dequeSize_ > 0) {
    const uint32_t back = (dequeFront_ + dequeSize_ - 1) % window_;
    if (dequeValues_[back] < required) break;
    dequeSize_ -= 1;
  }
  const uint32_t slot = (dequeFront_ + dequeSize_) % window_;
  dequeValues_[slot] = required;
  dequeFrames_[slot] = frame_;
  dequeSize_ += 1;
  frame_ += 1;
  return dequeValues_[dequeFront_];
}

void AudLimiterNode::renderFrames(const AudProcessContext&,
                                  const AudAudioBus& in, const AudAudioBus& out,
                                  uint32_t offset, uint32_t frames) {
  const uint32_t channels = std::min(out.num_channels, channels_);
  if (channels == 0 || window_ == 0) return;
  const uint32_t inputs = in.channels == nullptr ? 0 : in.num_channels;
  const float* sources[kLimiterMaxChannels];
  float* targets[kLimiterMaxChannels];
  for (uint32_t c = 0; c < channels; ++c) {
    // A mono input feeds every channel; a missing one is silent.
    sources[c] = c < inputs ? in.channels[c] + offset
                            : (inputs == 1 ? in.channels[0] + offset
                                           : silence());
    targets[c] = out.channels[c] + offset;
  }
  float samples[kLimiterMaxChannels];
  for (uint32_t start = 0; start < frames; start += kControlFrames) {
    const uint32_t end = std::min(frames, start + kControlFrames);
    const float span = static_cast<float>(end - start);
    const float gain0 = gain_.of(paramAt(AUD_LIMITER_PARAM_GAIN, start));
    const float gain1 = gainEnd_.of(paramAt(AUD_LIMITER_PARAM_GAIN, end));
    const float ceiling0 = ceiling_.of(paramAt(AUD_LIMITER_PARAM_CEILING, start));
    const float ceiling1 =
        ceilingEnd_.of(paramAt(AUD_LIMITER_PARAM_CEILING, end));
    const float release = paramAt(AUD_LIMITER_PARAM_RELEASE, start);
    if (release != release_) {
      release_ = release;
      releaseCoefficient_ = static_cast<float>(
          1.0 - std::exp(-1.0 / (static_cast<double>(release) * sampleRate())));
    }
    for (uint32_t i = start; i < end; ++i) {
      const float t = static_cast<float>(i - start) / span;
      const float gain = gain0 + (gain1 - gain0) * t;
      const float ceiling = ceiling0 + (ceiling1 - ceiling0) * t;
      // Every input of the frame is read before an output, which may alias
      // it.
      float peak = 0.0f;
      for (uint32_t c = 0; c < channels; ++c) {
        const float x = sources[c][i];
        samples[c] = std::isfinite(x) ? x * gain : 0.0f;
        if (!std::isfinite(samples[c])) samples[c] = 0.0f;
        peak = std::max(peak, std::fabs(samples[c]));
      }
      const float required = peak > ceiling ? ceiling / peak : 1.0f;
      const float hold = slidingMinimum(required);
      follower_ = hold < follower_
                      ? hold
                      : follower_ + (hold - follower_) * releaseCoefficient_;
      boxSum_ += static_cast<double>(follower_) - box_[boxPosition_];
      box_[boxPosition_] = follower_;
      if (++boxPosition_ == window_) {
        // The running sum starts again from the values, so that its
        // rounding never adds up.
        boxPosition_ = 0;
        boxSum_ = 0;
        for (float value : box_) boxSum_ += value;
      }
      const float smoothed = static_cast<float>(boxSum_ / window_);
      const uint32_t read =
          delayPosition_ + 1 == window_ ? 0 : delayPosition_ + 1;
      for (uint32_t c = 0; c < channels; ++c) {
        float* line = delay_.data() + static_cast<size_t>(c) * window_;
        line[delayPosition_] = samples[c];
        targets[c][i] = clampTo(line[read] * smoothed, -ceiling, ceiling);
      }
      delayPosition_ = read;
    }
  }
  for (uint32_t c = channels; c < out.num_channels; ++c) {
    std::fill(out.channels[c] + offset, out.channels[c] + offset + frames,
              0.0f);
  }
}

// ############################################################################
// The node type

namespace {

const AudNodeVTable kVTable = effectVTable<AudLimiterNode>();

const AudNodeDescriptor kDescriptor = effectDescriptor(
    AUD_DSP_EFFECTS_LIMITER_TYPE_ID, "Limiter",
    kCapabilities | AUD_NODE_CAP_LATENCY, kLimiterInputs, kLimiterOutputs,
    AUD_LIMITER_NUM_PARAMS, kLimiterParams, &kVTable);

}  // namespace

const AudNodeDescriptor& limiterDescriptor() { return kDescriptor; }

}  // namespace aud_effects
