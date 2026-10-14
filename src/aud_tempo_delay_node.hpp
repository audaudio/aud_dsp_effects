// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// aud.effects.tempo_delay (ticket 28): the delay lines of the stereo delay
// with the time of a note value at the tempo of the transport
// (AUD_NODE_CAP_TRANSPORT) - beats(division) * 60 / tempo of the segment at
// the start of the block, 120 BPM without a transport or a tempo - up to a
// buffer of 4 s. A change of the tempo or of the division glides like a
// change of the stereo delay's time.

#ifndef AUD_TEMPO_DELAY_NODE_HPP
#define AUD_TEMPO_DELAY_NODE_HPP

#include <cstdint>

#include "aud_dsp_effects.h"
#include "aud_stereo_delay_node.hpp"

namespace aud_effects {

// The parameters of the tempo delay by index (AUD_TEMPO_DELAY_PARAM_*):
// their ranges and defaults hold here and nowhere else.
constexpr AudParamDescriptor kTempoDelayParams[AUD_TEMPO_DELAY_NUM_PARAMS] = {
    {sizeof(AudParamDescriptor), "division", "Division", "", 0.0f,
     static_cast<float>(AUD_TEMPO_DELAY_NUM_DIVISIONS - 1),
     static_cast<float>(AUD_TEMPO_DELAY_DIVISION_8D), kStepped,
     AUD_TEMPO_DELAY_NUM_DIVISIONS, 0},
    {sizeof(AudParamDescriptor), "feedback", "Feedback", "", 0.0f, 1.0f, 0.3f,
     kRamped, 0, 0},
    {sizeof(AudParamDescriptor), "mix", "Mix", "", 0.0f, 1.0f, 0.25f, kRamped,
     0, 0},
    {sizeof(AudParamDescriptor), "ping_pong", "Ping-pong", "", 0.0f, 1.0f, 0.0f,
     kBoolean, 0, 0},
};

// The length of each division in quarter notes (AUD_TEMPO_DELAY_DIVISION_*),
// sorted.
constexpr double kDivisionBeats[AUD_TEMPO_DELAY_NUM_DIVISIONS] = {
    0.125,        // 1/32
    1.0 / 6.0,    // 1/16T
    0.1875,       // 1/32D
    0.25,         // 1/16
    1.0 / 3.0,    // 1/8T
    0.375,        // 1/16D
    0.5,          // 1/8
    2.0 / 3.0,    // 1/4T
    0.75,         // 1/8D
    1.0,          // 1/4
    4.0 / 3.0,    // 1/2T
    1.5,          // 1/4D
    2.0,          // 1/2
    8.0 / 3.0,    // 1/1T
    3.0,          // 1/2D
    4.0,          // 1/1
};

// The tempo without a transport, in beats per minute.
constexpr double kDefaultTempo = 120.0;

// One instance of aud.effects.tempo_delay.
class AudTempoDelayNode : public AudDelayNode {
 public:
  // Creates a node with the parameters at their defaults.
  AudTempoDelayNode(const AudNodeDescriptor* descriptor,
                    const AudHostApi* host);

 protected:
  AudDelayTime timeOf(const AudProcessContext& context) override;
  double tailSeconds() const override;

 private:
  // The time of the division at `tempo`, in seconds.
  double secondsAt(double tempo) const;

  double tempo_ = kDefaultTempo;  // the tempo of the last block
};

}  // namespace aud_effects

#endif  // AUD_TEMPO_DELAY_NODE_HPP
