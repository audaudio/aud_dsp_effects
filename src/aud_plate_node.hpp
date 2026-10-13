// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// aud.effects.plate (ticket 28): the plate reverb of J. Dattorro, "Effect
// Design, Part 1: Reverberator and Other Filters", J. Audio Eng. Soc. 45(9),
// 1997, figure 1 with the values of tables 1 and 2, written by Audanika from
// the paper - no third-party code. The sum of the input channels passes a
// pre-delay, the bandwidth one-pole and four input diffusers into the
// figure-eight tank: per half a modulated allpass (excursion 16 frames at
// 29761 Hz), a delay, the damping one-pole, the decay gain, a second
// allpass and a second delay, whose output, again times the decay, feeds
// the other half. Seven taps per side across both halves form the wet
// stereo signal (table 2), times 0.6. Every delay is scaled from 29761 Hz to
// the sample rate. The LFO is a rotating vector, not a sine per frame.

#ifndef AUD_PLATE_NODE_HPP
#define AUD_PLATE_NODE_HPP

#include <cstdint>
#include <vector>

#include "aud_dsp_effects.h"
#include "aud_effects_node.hpp"

namespace aud_effects {

// The parameters of the node by index (AUD_PLATE_PARAM_*): their ranges
// and defaults hold here and nowhere else; the defaults of bandwidth, decay
// and damping are the paper's (table 1).
constexpr AudParamDescriptor kPlateParams[AUD_PLATE_NUM_PARAMS] = {
    {sizeof(AudParamDescriptor), "pre_delay", "Pre-delay", "s", 0.0f, 0.25f,
     0.02f, kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "bandwidth", "Bandwidth", "", 0.0f, 1.0f,
     0.9995f, kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "decay", "Decay", "", 0.0f, 0.99f, 0.5f,
     kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "damping", "Damping", "", 0.0f, 1.0f, 0.0005f,
     kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "mix", "Mix", "", 0.0f, 1.0f, 0.25f, kRamped,
     0, 0},
};

// A delay line of a fixed length over a slice of the node's memory.
struct AudPlateDelay {
  float* data = nullptr;
  uint32_t size = 0;
  uint32_t position = 0;  // where the next frame is written

  // Writes `x` and returns the frame written `size` frames before.
  float process(float x) {
    const float y = data[position];
    data[position] = x;
    if (++position == size) position = 0;
    return y;
  }

  // The frame written `t` frames ago, 1 <= t <= size: 1 is the last one.
  float tap(uint32_t t) const {
    return data[position >= t ? position - t : position + size - t];
  }

  void clear();
};

// An allpass of Dattorro's figure: w = x - k * w[n - D], y = k * w + w[n - D].
struct AudPlateAllpass {
  AudPlateDelay line;  // holds w

  float process(float x, float k) {
    const float delayed = line.data[line.position];
    const float w = flush(x - k * delayed);
    line.process(w);
    return k * w + delayed;
  }
};

// The allpass with a delay that moves, read with linear interpolation.
struct AudPlateModulatedAllpass {
  AudPlateDelay line;  // holds w; longer than the longest delay

  float process(float x, float k, double delay) {
    double read = static_cast<double>(line.position) - delay;
    if (read < 0) read += line.size;
    uint32_t i0 = static_cast<uint32_t>(read);
    const float fraction = static_cast<float>(read - i0);
    if (i0 >= line.size) i0 -= line.size;
    const uint32_t i1 = i0 + 1 == line.size ? 0 : i0 + 1;
    const float delayed =
        line.data[i0] + fraction * (line.data[i1] - line.data[i0]);
    const float w = flush(x - k * delayed);
    line.process(w);
    return k * w + delayed;
  }
};

// The pre-delay: written first, then read `delay` frames back, 0 included.
struct AudPlatePreDelay {
  AudPlateDelay line;

  float process(float x, double delay) {
    line.data[line.position] = x;
    double read = static_cast<double>(line.position) - delay;
    if (read < 0) read += line.size;
    uint32_t i0 = static_cast<uint32_t>(read);
    const float fraction = static_cast<float>(read - i0);
    if (i0 >= line.size) i0 -= line.size;
    const uint32_t i1 = i0 + 1 == line.size ? 0 : i0 + 1;
    if (++line.position == line.size) line.position = 0;
    return line.data[i0] + fraction * (line.data[i1] - line.data[i0]);
  }
};

// The delays, filters and taps of the plate at one sample rate.
struct AudPlateTank {
  AudPlatePreDelay preDelay;
  AudPlateAllpass diffusers[4];
  AudPlateModulatedAllpass leftModulated;
  AudPlateDelay leftDelay1;
  AudPlateAllpass leftAllpass;
  AudPlateDelay leftDelay2;
  AudPlateModulatedAllpass rightModulated;
  AudPlateDelay rightDelay1;
  AudPlateAllpass rightAllpass;
  AudPlateDelay rightDelay2;
  // The nominal delays of the modulated allpasses and their excursion.
  double leftModulatedDelay = 0;
  double rightModulatedDelay = 0;
  double excursion = 0;
  // The taps of table 2, scaled.
  uint32_t taps[14] = {};
};

// One instance of aud.effects.plate.
class AudPlateNode : public AudEffectNode {
 public:
  // Creates a node with the parameters at their defaults.
  AudPlateNode(const AudNodeDescriptor* descriptor, const AudHostApi* host);

 protected:
  int32_t allocate(const AudPrepareInfo& info, uint32_t inputs,
                   uint32_t outputs) override;
  void clear() override;
  void renderFrames(const AudProcessContext& context, const AudAudioBus& in,
                    const AudAudioBus& out, uint32_t offset,
                    uint32_t frames) override;
  uint32_t computeTail() const override;

 private:
  std::vector<float> memory_;
  AudPlateTank tank_;
  AudQuadratureOscillator lfo_;
  float bandwidthState_ = 0;
  float leftDamping_ = 0;
  float rightDamping_ = 0;
  float leftOut_ = 0;
  float rightOut_ = 0;
};

}  // namespace aud_effects

#endif  // AUD_PLATE_NODE_HPP
