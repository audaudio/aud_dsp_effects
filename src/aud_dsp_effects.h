// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// The entry point of aud_dsp_effects (ticket 5, S0-mobile): registers the
// package's node types with an engine through the C ABI of aud_audio_core.
// The spike ships one node, a tremolo; ticket S10a adds reverb and delay.

#ifndef AUD_DSP_EFFECTS_H
#define AUD_DSP_EFFECTS_H

#include <stdint.h>

#include "aud_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

// The type id of the tremolo node: a gain modulated by a sine LFO with the
// parameters rate (Hz) and depth (0..1).
#define AUD_DSP_EFFECTS_TREMOLO_TYPE_ID "aud.effects.tremolo"

// [control] Registers the node types with the host api of an engine
// (`const AudHostApi*`); AUD_OK or the first error code.
AUD_EXPORT int32_t aud_dsp_effects_register(const void* host_api);

#ifdef __cplusplus
}
#endif

#endif  // AUD_DSP_EFFECTS_H
