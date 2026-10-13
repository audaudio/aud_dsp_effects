// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_plate_node.hpp"

#include <algorithm>
#include <cmath>

namespace aud_effects {

namespace {

// ############################################################################
// The values of the paper (tables 1 and 2), in frames at 29761 Hz

constexpr double kPlateRate = 29761.0;

constexpr uint32_t kDiffuserFrames[4] = {142, 107, 379, 277};
constexpr float kInputDiffusion1 = 0.75f;
constexpr float kInputDiffusion2 = 0.625f;
constexpr float kDecayDiffusion1 = 0.70f;

constexpr uint32_t kLeftModulatedFrames = 672;
constexpr uint32_t kLeftDelay1Frames = 4453;
constexpr uint32_t kLeftAllpassFrames = 1800;
constexpr uint32_t kLeftDelay2Frames = 3720;
constexpr uint32_t kRightModulatedFrames = 908;
constexpr uint32_t kRightDelay1Frames = 4217;
constexpr uint32_t kRightAllpassFrames = 2656;
constexpr uint32_t kRightDelay2Frames = 3163;

// The peak excursion of the modulated allpasses and the rate of their LFO.
constexpr double kExcursionFrames = 16.0;
constexpr double kLfoHz = 1.0;

// The gain of the taps.
constexpr float kOutputGain = 0.6f;

// The taps of table 2: the left output from 0 to 6, the right one from 7 to
// 13, in the order of the paper.
constexpr uint32_t kTapFrames[14] = {
    266, 2974, 1913, 1996, 1990, 187, 1066,  // left
    353, 3627, 1228, 2673, 2111, 335, 121,   // right
};

// The longest pre-delay in seconds.
constexpr double kMaxPreDelaySeconds = 0.25;

// `frames` at 29761 Hz scaled to `rate`, at least 1.
uint32_t scaled(uint32_t frames, double rate) {
  const double value = std::floor(frames * rate / kPlateRate + 0.5);
  return value < 1 ? 1 : static_cast<uint32_t>(value);
}

// The frames of one trip around the figure eight at `rate`.
double loopFrames(double rate) {
  return static_cast<double>(
      scaled(kLeftModulatedFrames, rate) + scaled(kLeftDelay1Frames, rate) +
      scaled(kLeftAllpassFrames, rate) + scaled(kLeftDelay2Frames, rate) +
      scaled(kRightModulatedFrames, rate) + scaled(kRightDelay1Frames, rate) +
      scaled(kRightAllpassFrames, rate) + scaled(kRightDelay2Frames, rate));
}

// Hands `size` floats of `memory` from `next` to `line`.
void slice(AudPlateDelay& line, uint32_t size, float* memory, size_t& next) {
  line.data = memory == nullptr ? nullptr : memory + next;
  line.size = size;
  line.position = 0;
  next += size;
}

// Lays the tank out on `memory`, or only counts its floats into `next` when
// `memory` is null.
void layout(AudPlateTank& tank, double rate, float* memory, size_t& next) {
  const uint32_t excursion =
      static_cast<uint32_t>(std::ceil(kExcursionFrames * rate / kPlateRate));
  const uint32_t preDelay =
      static_cast<uint32_t>(std::ceil(kMaxPreDelaySeconds * rate)) + 2;
  slice(tank.preDelay.line, preDelay, memory, next);
  for (uint32_t i = 0; i < 4; ++i) {
    slice(tank.diffusers[i].line, scaled(kDiffuserFrames[i], rate), memory,
          next);
  }
  tank.leftModulatedDelay = scaled(kLeftModulatedFrames, rate);
  tank.rightModulatedDelay = scaled(kRightModulatedFrames, rate);
  tank.excursion = kExcursionFrames * rate / kPlateRate;
  slice(tank.leftModulated.line,
        scaled(kLeftModulatedFrames, rate) + excursion + 2, memory, next);
  slice(tank.leftDelay1, scaled(kLeftDelay1Frames, rate), memory, next);
  slice(tank.leftAllpass.line, scaled(kLeftAllpassFrames, rate), memory, next);
  slice(tank.leftDelay2, scaled(kLeftDelay2Frames, rate), memory, next);
  slice(tank.rightModulated.line,
        scaled(kRightModulatedFrames, rate) + excursion + 2, memory, next);
  slice(tank.rightDelay1, scaled(kRightDelay1Frames, rate), memory, next);
  slice(tank.rightAllpass.line, scaled(kRightAllpassFrames, rate), memory,
        next);
  slice(tank.rightDelay2, scaled(kRightDelay2Frames, rate), memory, next);
  for (uint32_t i = 0; i < 14; ++i) tank.taps[i] = scaled(kTapFrames[i], rate);
}

}  // namespace

// ############################################################################
// AudPlateDelay

void AudPlateDelay::clear() {
  if (data != nullptr) std::fill(data, data + size, 0.0f);
  position = 0;
}

// ############################################################################
// AudPlateNode

AudPlateNode::AudPlateNode(const AudNodeDescriptor*, const AudHostApi*)
    : AudEffectNode(kPlateParams, AUD_PLATE_NUM_PARAMS) {}

int32_t AudPlateNode::allocate(const AudPrepareInfo& info, uint32_t,
                               uint32_t) {
  AudPlateTank tank;
  size_t floats = 0;
  layout(tank, info.sample_rate, nullptr, floats);
  std::vector<float> memory(floats, 0.0f);
  size_t next = 0;
  layout(tank, info.sample_rate, memory.data(), next);
  // The swap keeps the floats where the tank points.
  memory_.swap(memory);
  tank_ = tank;
  lfo_.setFrequency(kLfoHz, info.sample_rate);
  return AUD_OK;
}

void AudPlateNode::clear() {
  AudPlateTank& t = tank_;
  t.preDelay.line.clear();
  for (AudPlateAllpass& diffuser : t.diffusers) diffuser.line.clear();
  t.leftModulated.line.clear();
  t.leftDelay1.clear();
  t.leftAllpass.line.clear();
  t.leftDelay2.clear();
  t.rightModulated.line.clear();
  t.rightDelay1.clear();
  t.rightAllpass.line.clear();
  t.rightDelay2.clear();
  lfo_.restart();
  bandwidthState_ = 0;
  leftDamping_ = 0;
  rightDamping_ = 0;
  leftOut_ = 0;
  rightOut_ = 0;
}

void AudPlateNode::renderFrames(const AudProcessContext&,
                                const AudAudioBus& in, const AudAudioBus& out,
                                uint32_t offset, uint32_t frames) {
  if (out.num_channels < 2 || memory_.empty()) return;
  const float* inLeft = nullptr;
  const float* inRight = nullptr;
  inputPair(in, offset, inLeft, inRight);
  float* outLeft = out.channels[0] + offset;
  float* outRight = out.channels[1] + offset;
  AudPlateTank& t = tank_;
  const double rate = sampleRate();
  for (uint32_t i = 0; i < frames; ++i) {
    // Both inputs are read before the outputs, which may alias them.
    const float dryLeft = inLeft[i];
    const float dryRight = inRight[i];
    const double preDelay = paramAt(AUD_PLATE_PARAM_PRE_DELAY, i) * rate;
    const float bandwidth = paramAt(AUD_PLATE_PARAM_BANDWIDTH, i);
    const float decay = paramAt(AUD_PLATE_PARAM_DECAY, i);
    const float damping = paramAt(AUD_PLATE_PARAM_DAMPING, i);
    const float mix = paramAt(AUD_PLATE_PARAM_MIX, i);
    // Table 1: decay diffusion 2 follows the decay, from 0.25 to 0.5.
    const float decayDiffusion2 = clampTo(decay + 0.15f, 0.25f, 0.5f);

    // The input: pre-delay, bandwidth and the four diffusers.
    float x = t.preDelay.process(0.5f * (dryLeft + dryRight), preDelay);
    bandwidthState_ =
        flush(bandwidth * x + (1.0f - bandwidth) * bandwidthState_);
    x = t.diffusers[0].process(bandwidthState_, kInputDiffusion1);
    x = t.diffusers[1].process(x, kInputDiffusion1);
    x = t.diffusers[2].process(x, kInputDiffusion2);
    x = t.diffusers[3].process(x, kInputDiffusion2);

    // The tank: each half takes the input and the other half's output.
    const float leftIn = x + decay * rightOut_;
    const float rightIn = x + decay * leftOut_;
    float left = t.leftModulated.process(
        leftIn, -kDecayDiffusion1,
        t.leftModulatedDelay + t.excursion * lfo_.sine());
    left = t.leftDelay1.process(left);
    leftDamping_ = flush((1.0f - damping) * left + damping * leftDamping_);
    left = t.leftAllpass.process(leftDamping_ * decay, decayDiffusion2);
    leftOut_ = t.leftDelay2.process(flush(left));
    float right = t.rightModulated.process(
        rightIn, -kDecayDiffusion1,
        t.rightModulatedDelay + t.excursion * lfo_.cosine());
    right = t.rightDelay1.process(right);
    rightDamping_ = flush((1.0f - damping) * right + damping * rightDamping_);
    right = t.rightAllpass.process(rightDamping_ * decay, decayDiffusion2);
    rightOut_ = t.rightDelay2.process(flush(right));
    lfo_.advance();

    // The output taps of table 2.
    const uint32_t* k = t.taps;
    const float wetLeft =
        kOutputGain *
        (t.rightDelay1.tap(k[0]) + t.rightDelay1.tap(k[1]) -
         t.rightAllpass.line.tap(k[2]) + t.rightDelay2.tap(k[3]) -
         t.leftDelay1.tap(k[4]) - t.leftAllpass.line.tap(k[5]) -
         t.leftDelay2.tap(k[6]));
    const float wetRight =
        kOutputGain *
        (t.leftDelay1.tap(k[7]) + t.leftDelay1.tap(k[8]) -
         t.leftAllpass.line.tap(k[9]) + t.leftDelay2.tap(k[10]) -
         t.rightDelay1.tap(k[11]) - t.rightAllpass.line.tap(k[12]) -
         t.rightDelay2.tap(k[13]));
    outLeft[i] = flush((1.0f - mix) * dryLeft + mix * wetLeft);
    outRight[i] = flush((1.0f - mix) * dryRight + mix * wetRight);
  }
  lfo_.renormalize();
}

// The pre-delay, the input diffusers and the trips around the figure eight
// until the tank has decayed by 60 dB: one trip passes the decay gain four
// times; one more trip for the taps.
uint32_t AudPlateNode::computeTail() const {
  const double rate = sampleRate();
  const double decay = targetOf(AUD_PLATE_PARAM_DECAY);
  const double trips =
      decay > 0 ? std::log(0.001) / (4.0 * std::log(decay)) : 0.0;
  double input = targetOf(AUD_PLATE_PARAM_PRE_DELAY) * rate;
  for (uint32_t frames : kDiffuserFrames) input += scaled(frames, rate);
  return tailFrames(input + loopFrames(rate) * (trips + 1.0), rate);
}

// ############################################################################
// The node type

namespace {

const AudNodeVTable kVTable = effectVTable<AudPlateNode>();

const AudNodeDescriptor kDescriptor = effectDescriptor(
    AUD_DSP_EFFECTS_PLATE_TYPE_ID, "Plate reverb", kCapabilities,
    kStereoInputs, kStereoOutputs, AUD_PLATE_NUM_PARAMS, kPlateParams,
    &kVTable);

}  // namespace

const AudNodeDescriptor& plateDescriptor() { return kDescriptor; }

}  // namespace aud_effects
