// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// aud.effects.freeverb (ticket 28): Jezar's Freeverb, public domain, from
// src/third_party/freeverb - eight parallel combs and four allpasses in
// series per side, fed by the sum of both input channels, the tunings
// scaled from 44.1 kHz to the sample rate and the buffers allocated in
// prepare. Freeverb's freeze mode is left out.

#ifndef AUD_FREEVERB_NODE_HPP
#define AUD_FREEVERB_NODE_HPP

#include <cstdint>
#include <vector>

#include "aud_dsp_effects.h"
#include "aud_effects_node.hpp"
#include "revmodel.hpp"

namespace aud_effects {

// The parameters of the node by index (AUD_FREEVERB_PARAM_*): their ranges
// and defaults hold here and nowhere else.
constexpr AudParamDescriptor kFreeverbParams[AUD_FREEVERB_NUM_PARAMS] = {
    {sizeof(AudParamDescriptor), "room_size", "Room size", "", 0.0f, 1.0f,
     0.5f, kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "damping", "Damping", "", 0.0f, 1.0f, 0.5f,
     kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "width", "Width", "", 0.0f, 1.0f, 1.0f,
     kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "mix", "Mix", "", 0.0f, 1.0f, 0.25f, kRamped,
     0, 0},
};

// One instance of aud.effects.freeverb.
class AudFreeverbNode : public AudEffectNode {
 public:
  // Creates a node with the parameters at their defaults.
  AudFreeverbNode(const AudNodeDescriptor* descriptor, const AudHostApi* host);

 protected:
  int32_t allocate(const AudPrepareInfo& info, uint32_t inputs,
                   uint32_t outputs) override;
  void clear() override;
  void renderFrames(const AudProcessContext& context, const AudAudioBus& in,
                    const AudAudioBus& out, uint32_t offset,
                    uint32_t frames) override;
  uint32_t computeTail() const override;

 private:
  // Hands the room size, the damping and the width at frame `i` of the
  // sub-range to the model.
  void applyParams(uint32_t i);

  std::vector<float> memory_;
  revmodel model_;
  // The values the model holds.
  float room_ = -1.0f;
  float damping_ = -1.0f;
  float width_ = -1.0f;
};

}  // namespace aud_effects

#endif  // AUD_FREEVERB_NODE_HPP
