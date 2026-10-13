// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// aud.effects.tremolo (ticket 5, on AudNodeBase since ticket 28): a gain
// modulated by a sine LFO, 1 at the peak of the LFO and 1 - depth at its
// trough. The LFO is a rotating vector, not a sine per frame.

#ifndef AUD_TREMOLO_NODE_HPP
#define AUD_TREMOLO_NODE_HPP

#include <cstdint>

#include "aud_dsp_effects.h"
#include "aud_effects_node.hpp"

namespace aud_effects {

// The parameters of the node by index (AUD_TREMOLO_PARAM_*): their ranges
// and defaults hold here and nowhere else.
constexpr AudParamDescriptor kTremoloParams[AUD_TREMOLO_NUM_PARAMS] = {
    {sizeof(AudParamDescriptor), "rate", "Rate", "Hz", 0.1f, 20.0f, 5.0f,
     kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "depth", "Depth", "", 0.0f, 1.0f, 0.5f,
     kRamped, 0, 0},
};

// One instance of aud.effects.tremolo.
class AudTremoloNode : public AudEffectNode {
 public:
  // Creates a node with the parameters at their defaults.
  AudTremoloNode(const AudNodeDescriptor* descriptor, const AudHostApi* host);

 protected:
  int32_t allocate(const AudPrepareInfo& info, uint32_t inputs,
                   uint32_t outputs) override;
  void clear() override;
  void renderFrames(const AudProcessContext& context, const AudAudioBus& in,
                    const AudAudioBus& out, uint32_t offset,
                    uint32_t frames) override;

 private:
  AudQuadratureOscillator lfo_;
  float rate_ = -1.0f;  // the rate the LFO runs at
};

}  // namespace aud_effects

#endif  // AUD_TREMOLO_NODE_HPP
