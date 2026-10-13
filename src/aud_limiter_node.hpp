// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// aud.effects.limiter (ticket 28): a look-ahead peak limiter of Audanika's
// own, after the peak limiters of D. Giannoulis, M. Massberg and J. D.
// Reiss, "Digital Dynamic Range Compressor Design - A Tutorial and
// Analysis", JAES 60(6), 2012, and U. Zoelzer, DAFX, chapter 4 (dynamics
// processing): no output sample exceeds the ceiling, for any input.
//
// With the look-ahead L = round(1.5 ms * sample rate) and W = L + 1 frames:
//
// 1. x' = input * gain, a non-finite sample counts as 0;
// 2. r[n] = min(1, ceiling / max over the channels |x'[n]|), linked over
//    all channels;
// 3. h[n] = min(r[n - W + 1 .. n]), a sliding minimum on a monotonic deque;
// 4. e[n] = h[n] when it falls, else e[n-1] + (h[n] - e[n-1]) * a with the
//    release coefficient a - so e[n] <= h[n];
// 5. s[n] = the mean of e[n - W + 1 .. n], a box filter;
// 6. y[n] = x'[n - L] * s[n], clamped to the ceiling for safety.
//
// Every e[n - k] of the mean is at most r[n - L], since each window of
// step 3 that ends at n - k, 0 <= k <= L, holds n - L: the gain meets every
// peak fully reduced, and the box filter turns the reduction into a ramp of
// L frames. The signal is delayed by exactly L frames, the latency the node
// reports and its tail. L is fixed at prepare (the engine reads the latency
// once).

#ifndef AUD_LIMITER_NODE_HPP
#define AUD_LIMITER_NODE_HPP

#include <cstdint>
#include <vector>

#include "aud_dsp_effects.h"
#include "aud_effects_node.hpp"

namespace aud_effects {

// The parameters of the node by index (AUD_LIMITER_PARAM_*): their ranges
// and defaults hold here and nowhere else.
constexpr AudParamDescriptor kLimiterParams[AUD_LIMITER_NUM_PARAMS] = {
    {sizeof(AudParamDescriptor), "ceiling", "Ceiling", "dB", -24.0f, 0.0f,
     -1.0f, kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "release", "Release", "s", 0.001f, 2.0f, 0.1f,
     kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "gain", "Gain", "dB", -24.0f, 24.0f, 0.0f,
     kRamped, 0, 0},
};

// The look-ahead in seconds.
constexpr double kLookaheadSeconds = 0.0015;

// The most channels the limiter takes.
constexpr uint32_t kLimiterMaxChannels = 16;

// The buses of the limiter: 1 to 16 channels.
constexpr AudBusDescriptor kLimiterInputs[] = {
    {sizeof(AudBusDescriptor), "in", "Input", AUD_BUS_MAIN, 1,
     kLimiterMaxChannels, 2},
};
constexpr AudBusDescriptor kLimiterOutputs[] = {
    {sizeof(AudBusDescriptor), "out", "Output", AUD_BUS_MAIN, 1,
     kLimiterMaxChannels, 2},
};

// The look-ahead in frames at `sampleRate`, at least 1.
uint32_t limiterLookahead(double sampleRate);

// One instance of aud.effects.limiter.
class AudLimiterNode : public AudEffectNode {
 public:
  // Creates a node with the parameters at their defaults.
  AudLimiterNode(const AudNodeDescriptor* descriptor, const AudHostApi* host);

  // [control] The look-ahead in frames, by which the node delays.
  uint32_t latency() const override { return lookahead_; }

 protected:
  int32_t allocate(const AudPrepareInfo& info, uint32_t inputs,
                   uint32_t outputs) override;
  void clear() override;
  void renderFrames(const AudProcessContext& context, const AudAudioBus& in,
                    const AudAudioBus& out, uint32_t offset,
                    uint32_t frames) override;
  uint32_t computeTail() const override { return lookahead_; }

 private:
  // A linear gain of a dB value, computed again only when it changes.
  struct Decibels {
    float db = 1e30f;
    float linear = 1.0f;
    float of(float value);
  };

  // Pushes the required gain of frame `frame` and returns the minimum of
  // the window.
  float slidingMinimum(float required);

  uint32_t lookahead_ = 0;
  uint32_t window_ = 0;  // lookahead_ + 1
  uint32_t channels_ = 0;
  // The delayed signal, window_ frames per channel.
  std::vector<float> delay_;
  uint32_t delayPosition_ = 0;
  // The monotonic deque of the sliding minimum: values rising from front
  // to back with the frames they were pushed at.
  std::vector<float> dequeValues_;
  std::vector<uint64_t> dequeFrames_;
  uint32_t dequeFront_ = 0;
  uint32_t dequeSize_ = 0;
  uint64_t frame_ = 0;
  // The release follower and the box filter over it.
  float follower_ = 1.0f;
  std::vector<float> box_;
  uint32_t boxPosition_ = 0;
  double boxSum_ = 0;
  // The parameters in their linear form.
  Decibels gain_;
  Decibels ceiling_;
  Decibels ceilingEnd_;
  Decibels gainEnd_;
  float release_ = -1.0f;
  float releaseCoefficient_ = 1.0f;
};

}  // namespace aud_effects

#endif  // AUD_LIMITER_NODE_HPP
