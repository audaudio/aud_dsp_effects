// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// The entry point of aud_dsp_effects (step S10a, ticket 28): registers the
// effect nodes of the package with an engine through the C ABI of
// aud_audio_core (ABI 0.4), and a bench that times a node on the calling
// thread.
//
// Every node renders its main input bus in place, takes any block up to the
// prepared maximum, ramps its parameters through AUD_EVENT_PARAM events and
// allocates only in prepare. The reverbs and the delays take a mono or a
// stereo input - a mono input feeds both sides - and render stereo; `mix` is
// a linear crossfade from the dry (0) to the wet signal (1). The limiter
// takes 1 to 16 channels.

#ifndef AUD_DSP_EFFECTS_H
#define AUD_DSP_EFFECTS_H

#include <stdint.h>

#include "aud_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

// ............................................................................
// Type ids

// Jezar's Freeverb (public domain): eight combs and four allpasses per side,
// the tunings scaled from 44.1 kHz to the sample rate.
#define AUD_DSP_EFFECTS_FREEVERB_TYPE_ID "aud.effects.freeverb"

// The plate reverb of J. Dattorro, "Effect Design, Part 1", JAES 45(9), 1997,
// written from the paper; the delays scaled from 29761 Hz.
#define AUD_DSP_EFFECTS_PLATE_TYPE_ID "aud.effects.plate"

// DunneAudioKit's StereoDelay (MIT), the time in seconds.
#define AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID "aud.effects.stereo_delay"

// The delay lines of the stereo delay with the time of a note value at the
// tempo of the transport.
#define AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID "aud.effects.tempo_delay"

// A look-ahead peak limiter: no output sample exceeds the ceiling.
#define AUD_DSP_EFFECTS_LIMITER_TYPE_ID "aud.effects.limiter"

// A gain modulated by a sine LFO.
#define AUD_DSP_EFFECTS_TREMOLO_TYPE_ID "aud.effects.tremolo"

// ............................................................................
// Parameters by index; their ids are stable

// aud.effects.freeverb
enum {
  // "room_size": the feedback of the combs, 0 to 1; default 0.5.
  AUD_FREEVERB_PARAM_ROOM_SIZE = 0,
  // "damping": the low-pass in the combs, 0 to 1; default 0.5.
  AUD_FREEVERB_PARAM_DAMPING = 1,
  // "width": the stereo width of the wet signal, 0 to 1; default 1.
  AUD_FREEVERB_PARAM_WIDTH = 2,
  // "mix": dry (0) to wet (1); default 0.25.
  AUD_FREEVERB_PARAM_MIX = 3,
  AUD_FREEVERB_NUM_PARAMS = 4,
};

// aud.effects.plate
enum {
  // "pre_delay": the delay before the reverb in seconds, 0 to 0.25;
  // default 0.02.
  AUD_PLATE_PARAM_PRE_DELAY = 0,
  // "bandwidth": the input low-pass, 0 to 1 (1 passes all); default
  // 0.9995.
  AUD_PLATE_PARAM_BANDWIDTH = 1,
  // "decay": the gain of the tank, 0 to 0.99; default 0.5.
  AUD_PLATE_PARAM_DECAY = 2,
  // "damping": the low-pass in the tank, 0 (none) to 1; default 0.0005.
  AUD_PLATE_PARAM_DAMPING = 3,
  // "mix": dry (0) to wet (1); default 0.25.
  AUD_PLATE_PARAM_MIX = 4,
  AUD_PLATE_NUM_PARAMS = 5,
};

// aud.effects.stereo_delay
enum {
  // "time": the delay in seconds, 0.001 to 2; default 0.4.
  AUD_STEREO_DELAY_PARAM_TIME = 0,
  // "feedback": 0 to 1; default 0.3.
  AUD_STEREO_DELAY_PARAM_FEEDBACK = 1,
  // "mix": dry (0) to wet (1); default 0.25.
  AUD_STEREO_DELAY_PARAM_MIX = 2,
  // "ping_pong": 0 or 1; the echoes alternate between the sides.
  AUD_STEREO_DELAY_PARAM_PING_PONG = 3,
  AUD_STEREO_DELAY_NUM_PARAMS = 4,
};

// aud.effects.tempo_delay
enum {
  // "division": the note value, an index into the divisions below; default
  // AUD_TEMPO_DELAY_DIVISION_8D.
  AUD_TEMPO_DELAY_PARAM_DIVISION = 0,
  // "feedback": 0 to 1; default 0.3.
  AUD_TEMPO_DELAY_PARAM_FEEDBACK = 1,
  // "mix": dry (0) to wet (1); default 0.25.
  AUD_TEMPO_DELAY_PARAM_MIX = 2,
  // "ping_pong": 0 or 1; the echoes alternate between the sides.
  AUD_TEMPO_DELAY_PARAM_PING_PONG = 3,
  AUD_TEMPO_DELAY_NUM_PARAMS = 4,
};

// The note values of the tempo delay, sorted by length; T is a triplet, D a
// dotted note.
enum {
  AUD_TEMPO_DELAY_DIVISION_32 = 0,
  AUD_TEMPO_DELAY_DIVISION_16T = 1,
  AUD_TEMPO_DELAY_DIVISION_32D = 2,
  AUD_TEMPO_DELAY_DIVISION_16 = 3,
  AUD_TEMPO_DELAY_DIVISION_8T = 4,
  AUD_TEMPO_DELAY_DIVISION_16D = 5,
  AUD_TEMPO_DELAY_DIVISION_8 = 6,
  AUD_TEMPO_DELAY_DIVISION_4T = 7,
  AUD_TEMPO_DELAY_DIVISION_8D = 8,
  AUD_TEMPO_DELAY_DIVISION_4 = 9,
  AUD_TEMPO_DELAY_DIVISION_2T = 10,
  AUD_TEMPO_DELAY_DIVISION_4D = 11,
  AUD_TEMPO_DELAY_DIVISION_2 = 12,
  AUD_TEMPO_DELAY_DIVISION_1T = 13,
  AUD_TEMPO_DELAY_DIVISION_2D = 14,
  AUD_TEMPO_DELAY_DIVISION_1 = 15,
  AUD_TEMPO_DELAY_NUM_DIVISIONS = 16,
};

// aud.effects.limiter
enum {
  // "ceiling": the largest output in dBFS, -24 to 0; default -1.
  AUD_LIMITER_PARAM_CEILING = 0,
  // "release": the time the gain takes to recover in seconds, 0.001 to 2;
  // default 0.1.
  AUD_LIMITER_PARAM_RELEASE = 1,
  // "gain": the input gain in dB, -24 to 24; default 0.
  AUD_LIMITER_PARAM_GAIN = 2,
  AUD_LIMITER_NUM_PARAMS = 3,
};

// aud.effects.tremolo
enum {
  // "rate": the LFO in Hz, 0.1 to 20; default 5.
  AUD_TREMOLO_PARAM_RATE = 0,
  // "depth": 0 to 1; default 0.5.
  AUD_TREMOLO_PARAM_DEPTH = 1,
  AUD_TREMOLO_NUM_PARAMS = 2,
};

// ............................................................................
// Registration

// [control] Registers the node types with the host api of an engine; AUD_OK
// or the first error code. An AudRegisterFunction.
AUD_EXPORT int32_t aud_dsp_effects_register(const AudHostApi* host);

// ............................................................................
// Bench

// What aud_dsp_effects_bench_run measures: a node of `type_id` prepared for
// `sample_rate` and blocks of `frames`, with `channels` input channels and
// the parameters `param_ids` set to `param_values`.
typedef struct AudEffectsBenchRequest {
  uint32_t struct_size;
  // The frames of a block, at least 1.
  uint32_t frames;
  // The blocks that are timed, at least 1.
  uint32_t blocks;
  // The blocks rendered before the timed ones.
  uint32_t warmup_blocks;
  // The channels of the input bus; the limiter's output has as many.
  uint32_t channels;
  // The entries of param_ids and param_values.
  uint32_t num_params;
  double sample_rate;
  const char* type_id;
  const char* const* param_ids;
  const float* param_values;
} AudEffectsBenchRequest;

// The time the process calls of the timed blocks took.
typedef struct AudEffectsBenchResult {
  uint32_t struct_size;
  // The blocks that were timed.
  uint32_t blocks;
  // The duration of a block at the sample rate in nanoseconds.
  double block_ns;
  // The mean, the 99th percentile and the maximum of the process calls in
  // nanoseconds.
  double mean_ns;
  double p99_ns;
  double max_ns;
} AudEffectsBenchResult;

// [offline] Times a node of the package on the calling thread: registers
// the package with a minimal host, creates the node, prepares it, sets the
// parameters and renders `warmup_blocks` and then `blocks` blocks of a
// stereo test signal - a dense chord at -6 dBFS that decays into silence
// within every 4 seconds -, each process call timed with a steady clock.
// AUD_OK, AUD_ERROR_INVALID_ARGUMENT for a missing or short struct, no
// frames, no blocks, a channel count the node does not take or a sample rate
// outside 8 to 384 kHz, AUD_ERROR_UNKNOWN_TYPE for a type id the package
// does not register, AUD_ERROR_NOT_FOUND for an unknown parameter id, or the
// error of the node's prepare.
AUD_EXPORT int32_t aud_dsp_effects_bench_run(
    const AudEffectsBenchRequest* request, AudEffectsBenchResult* result);

#ifdef __cplusplus
}
#endif

#endif  // AUD_DSP_EFFECTS_H
