// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// aud.effects.stereo_delay (ticket 28): DunneAudioKit's StereoDelay (MIT,
// Copyright (c) 2021 AudioKit) from src/third_party/dunne_audio_kit, with
// the time in seconds up to 2 s, the feedback, the mix and the ping-pong
// mode of AudioKit's StereoDelayDSP. AudDelayNode drives the vendored delay
// frame by frame: the delay glides towards the time of each frame with a
// time constant of 50 ms, so that a jump of the time moves the read
// position like a tape instead of clicking; the feedback and the mix follow
// their ramps per frame. The tempo delay derives from it.

#ifndef AUD_STEREO_DELAY_NODE_HPP
#define AUD_STEREO_DELAY_NODE_HPP

#include <cstdint>
#include <memory>

#include "StereoDelay.h"
#include "aud_dsp_effects.h"
#include "aud_effects_node.hpp"

namespace aud_effects {

// The parameters of the stereo delay by index (AUD_STEREO_DELAY_PARAM_*):
// their ranges and defaults hold here and nowhere else.
constexpr AudParamDescriptor kStereoDelayParams[AUD_STEREO_DELAY_NUM_PARAMS] =
    {
        {sizeof(AudParamDescriptor), "time", "Time", "s", 0.001f, 2.0f, 0.4f,
         kRamped, 0, 0},
        {sizeof(AudParamDescriptor), "feedback", "Feedback", "", 0.0f, 1.0f,
         0.3f, kRamped, 0, 0},
        {sizeof(AudParamDescriptor), "mix", "Mix", "", 0.0f, 1.0f, 0.25f,
         kRamped, 0, 0},
        {sizeof(AudParamDescriptor), "ping_pong", "Ping-pong", "", 0.0f, 1.0f,
         0.0f, kBoolean, 0, 0},
};

// The shortest delay of both delay nodes in seconds.
constexpr double kMinDelaySeconds = 0.001;

// The time constant of the glide of the delay time in seconds.
constexpr double kDelayGlideSeconds = 0.05;

// The delay time of a sub-range in seconds: a ramp of a parameter or a
// constant.
struct AudDelayTime {
  float value = 0;
  float step = 0;
  uint32_t remaining = 0;
  float target = 0;

  // The time at frame `i` of the sub-range.
  float at(uint32_t i) const {
    return i < remaining ? value + step * static_cast<float>(i) : target;
  }
};

// A node on the vendored StereoDelay: the stereo delay and the tempo delay.
// The parameters at `feedback`, `mix` and `pingPong` are the delay's; the
// time comes from timeOf().
class AudDelayNode : public AudEffectNode {
 public:
  // Creates a node with `params` at their defaults and a buffer for
  // `maxSeconds`.
  AudDelayNode(const AudParamDescriptor* params, uint32_t numParams,
               uint32_t feedback, uint32_t mix, uint32_t pingPong,
               double maxSeconds);

 protected:
  int32_t allocate(const AudPrepareInfo& info, uint32_t inputs,
                   uint32_t outputs) override;
  void clear() override;
  void renderFrames(const AudProcessContext& context, const AudAudioBus& in,
                    const AudAudioBus& out, uint32_t offset,
                    uint32_t frames) override;
  uint32_t computeTail() const override;

  // [realtime] The delay time of the sub-range in seconds, before it is
  // clamped to the buffer.
  virtual AudDelayTime timeOf(const AudProcessContext& context) = 0;

  // The time the tail counts with, in seconds.
  virtual double tailSeconds() const = 0;

  // The longest delay in seconds.
  double maxSeconds() const { return maxSeconds_; }

 private:
  uint32_t feedback_;
  uint32_t mix_;
  uint32_t pingPong_;
  double maxSeconds_;
  std::unique_ptr<DunneCore::StereoDelay> delay_;
  AudSmoother glide_;  // the delay in frames
  bool snap_ = true;   // the next frame takes its delay without a glide
  bool pingPongOn_ = false;
};

// One instance of aud.effects.stereo_delay.
class AudStereoDelayNode : public AudDelayNode {
 public:
  // Creates a node with the parameters at their defaults.
  AudStereoDelayNode(const AudNodeDescriptor* descriptor,
                     const AudHostApi* host);

 protected:
  AudDelayTime timeOf(const AudProcessContext& context) override;
  double tailSeconds() const override;
};

}  // namespace aud_effects

#endif  // AUD_STEREO_DELAY_NODE_HPP
