// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_dsp_effects.h"

#include <cmath>
#include <cstdint>
#include <new>

namespace {

constexpr double kTwoPi = 6.283185307179586;

// ###########################################################################
// aud.effects.tremolo

struct TremoloNode {
  double sampleRate = 48000.0;
  double phase = 0.0;
  float rate = 5.0f;
  float depth = 0.5f;
};

const AudParamDescriptor kTremoloParams[] = {
    {sizeof(AudParamDescriptor), "rate", "Rate", "Hz", 0.1f, 20.0f, 5.0f},
    {sizeof(AudParamDescriptor), "depth", "Depth", "", 0.0f, 1.0f, 0.5f},
};

void* tremoloCreate(const AudNodeDescriptor*, const AudHostApi* host) {
  auto* node = new (std::nothrow) TremoloNode();
  if (node == nullptr && host != nullptr && host->log != nullptr) {
    host->log(host->host, AUD_LOG_ERROR, "aud.effects.tremolo: out of memory");
  }
  return node;
}

void tremoloDestroy(void* instance) { delete static_cast<TremoloNode*>(instance); }

int32_t tremoloPrepare(void* instance, double sampleRate, uint32_t, uint32_t) {
  auto* node = static_cast<TremoloNode*>(instance);
  node->sampleRate = sampleRate;
  node->phase = 0.0;
  return AUD_OK;
}

void tremoloReset(void* instance) {
  static_cast<TremoloNode*>(instance)->phase = 0.0;
}

void tremoloSetParam(void* instance, uint32_t index, float value) {
  auto* node = static_cast<TremoloNode*>(instance);
  if (index == 0) node->rate = value;
  if (index == 1) node->depth = value;
}

void tremoloProcess(void* instance, const AudProcessContext* context) {
  auto* node = static_cast<TremoloNode*>(instance);
  const double increment = kTwoPi * node->rate / node->sampleRate;
  double phase = node->phase;
  for (uint32_t frame = 0; frame < context->frames; ++frame) {
    // 1 at the LFO peak, 1 - depth at the trough.
    const float gain =
        1.0f - node->depth * 0.5f * (1.0f - static_cast<float>(std::sin(phase)));
    for (uint32_t channel = 0; channel < context->channels; ++channel) {
      context->outputs[channel][frame] = context->inputs[channel][frame] * gain;
    }
    phase += increment;
    if (phase >= kTwoPi) phase -= kTwoPi;
  }
  node->phase = phase;
}

const AudNodeVTable kTremoloVTable = {
    sizeof(AudNodeVTable), tremoloCreate,   tremoloDestroy, tremoloPrepare,
    tremoloReset,          tremoloSetParam, nullptr,        tremoloProcess,
    nullptr,
};

const AudNodeDescriptor kTremoloDescriptor = {
    sizeof(AudNodeDescriptor),
    AUD_ABI_VERSION_MAJOR,
    AUD_ABI_VERSION_MINOR,
    AUD_DSP_EFFECTS_TREMOLO_TYPE_ID,
    "Tremolo",
    AUD_NODE_CAP_IN_PLACE | AUD_NODE_CAP_VARIABLE_BLOCK,
    1,
    1,
    2,
    kTremoloParams,
    &kTremoloVTable,
};

}  // namespace

AUD_EXPORT int32_t aud_dsp_effects_register(const void* host_api) {
  const auto* host = static_cast<const AudHostApi*>(host_api);
  if (host == nullptr || host->struct_size < sizeof(AudHostApi) ||
      host->register_node_type == nullptr) {
    return AUD_ERROR_INVALID_ARGUMENT;
  }
  if (host->abi_major != AUD_ABI_VERSION_MAJOR) return AUD_ERROR_ABI_MAJOR;
  return host->register_node_type(host->host, &kTremoloDescriptor);
}
