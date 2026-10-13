// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// The native tests of the effect nodes on the graph engine (ticket 28): the
// registration, every node and the master chain of an app - delay, plate,
// limiter - rendered live and offline under the engine's watchdog, mono
// inputs, parameter ramps through the engine, the latency and the tail the
// engine reads, the tempo of the engine's transport, and the probe that
// proves the RealtimeSanitizer run catches an allocation.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "aud_effects_fixture.hpp"

using namespace aud_effects_test;

// ############################################################################
// Registration

AUD_TEST(registers_six_node_types_with_an_engine_once) {
  Graph g;
  for (const std::string& id : typeIds()) {
    const AudNodeDescriptor* d = aud_graph_node_type_by_id(g.graph, id.c_str());
    AUD_CHECK(d != nullptr);
    if (d != nullptr) AUD_CHECK(d->abi_major == 0 && d->abi_minor == 4);
  }
  // A second registration is refused like any duplicate type.
  AUD_CHECK(aud_dsp_effects_register(g.host()) == AUD_ERROR_DUPLICATE_TYPE);
  AUD_CHECK(aud_dsp_effects_register(nullptr) == AUD_ERROR_INVALID_ARGUMENT);
  AudHostApi other = *g.host();
  other.register_node_type = nullptr;
  AUD_CHECK(aud_dsp_effects_register(&other) == AUD_ERROR_INVALID_ARGUMENT);
  other = *g.host();
  other.abi_major = 1;
  AUD_CHECK(aud_dsp_effects_register(&other) == AUD_ERROR_ABI_MAJOR);
  other = *g.host();
  other.struct_size = 64;
  AUD_CHECK(aud_dsp_effects_register(&other) == AUD_ERROR_ABI_MINOR);
}

// ############################################################################
// Rendering on the engine

AUD_TEST(renders_every_node_on_the_engine) {
  for (const std::string& id : typeIds()) {
    Graph g;
    const int32_t node = g.node(id.c_str());
    g.chain({node});
    const Channels in = {chord(24000, 0.5f), chord(24000, 0.5f, kRate, 0.9)};
    const Channels live = g.renderLive(in);
    const Channels offline = g.renderOffline(in, 128);
    AUD_CHECK(allFinite(live) && allFinite(offline));
    AUD_CHECK(peak(live[0]) > 0.01f && peak(offline[1]) > 0.01f);
    AUD_CHECK(aud_graph_node_latency(g.graph, node) ==
              (id == AUD_DSP_EFFECTS_LIMITER_TYPE_ID ? 72 : 0));
  }
}

AUD_TEST(renders_the_master_chain_of_an_app) {
  // The master chain of the plan: delay, plate, limiter, with the moderate
  // defaults, and a dense chord pushed 12 dB over full scale.
  Graph g;
  const int32_t delay = g.node(AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID);
  const int32_t plate = g.node(AUD_DSP_EFFECTS_PLATE_TYPE_ID);
  const int32_t limiter = g.node(AUD_DSP_EFFECTS_LIMITER_TYPE_ID);
  g.param(delay, AUD_STEREO_DELAY_PARAM_TIME, 0.4f);
  g.param(delay, AUD_STEREO_DELAY_PARAM_FEEDBACK, 0.15f);
  g.param(delay, AUD_STEREO_DELAY_PARAM_MIX, 0.06f);
  g.param(delay, AUD_STEREO_DELAY_PARAM_PING_PONG, 1.0f);
  g.param(plate, AUD_PLATE_PARAM_MIX, 0.15f);
  g.param(plate, AUD_PLATE_PARAM_DECAY, 0.55f);
  g.chain({delay, plate, limiter});
  AUD_CHECK(aud_graph_output_latency(g.graph) == 72);
  AUD_CHECK(aud_graph_output_tail(g.graph) > 48000);
  const Channels in = {chord(96000, 4.0f), chord(96000, 4.0f, kRate, 0.9)};
  const Channels out = g.renderLive(in);
  // A preset changes the effects with a ramp while the chain renders.
  g.param(plate, AUD_PLATE_PARAM_MIX, 0.3f, 4800);
  g.param(delay, AUD_STEREO_DELAY_PARAM_TIME, 0.3f, 4800);
  g.param(limiter, AUD_LIMITER_PARAM_GAIN, 6.0f, 4800);
  const Channels more = g.renderLive(in);
  const float ceiling = ceilingOf(-1.0f);
  for (const Channels* rendered : {&out, &more}) {
    AUD_CHECK(allFinite(*rendered));
    for (const auto& channel : *rendered) {
      AUD_CHECK(peak(channel) <= ceiling);
      AUD_CHECK(peak(channel) > 0.5f * ceiling);
    }
  }
  AudGraphStats stats{};
  stats.struct_size = sizeof(AudGraphStats);
  AUD_CHECK(aud_graph_get_stats(g.graph, &stats) == AUD_OK);
  AUD_CHECK(stats.output_peak <= ceiling);
}

AUD_TEST(takes_a_mono_input_on_the_engine) {
  for (const std::string& id : typeIds()) {
    Graph g;
    const int32_t node = g.node(id.c_str(), 1, 0);
    AUD_CHECK(aud_graph_node_channels(g.graph, node, 0, 0) == 1);
    g.chain({node});
    const std::vector<float> signal = chord(9600, 0.5f);
    const Channels out = g.renderOffline({signal, signal});
    AUD_CHECK(allFinite(out));
    AUD_CHECK(peak(out[0]) > 0.01f && peak(out[1]) > 0.01f);
  }
}

AUD_TEST(reports_the_tail_to_the_engine_at_every_commit) {
  Graph g;
  const int32_t plate = g.node(AUD_DSP_EFFECTS_PLATE_TYPE_ID);
  g.chain({plate});
  const uint32_t before = aud_graph_output_tail(g.graph);
  g.param(plate, AUD_PLATE_PARAM_DECAY, 0.8f);
  g.renderOffline({silence(256), silence(256)});
  AUD_CHECK(aud_graph_begin(g.graph) == AUD_OK);
  AUD_CHECK(aud_graph_commit(g.graph) > 0);
  AUD_CHECK(aud_graph_output_tail(g.graph) > before);
}

AUD_TEST(tempo_delay_follows_the_tempo_of_the_engine) {
  Graph g;
  const int32_t delay = g.node(AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID);
  g.param(delay, AUD_TEMPO_DELAY_PARAM_FEEDBACK, 0.0f);
  g.param(delay, AUD_TEMPO_DELAY_PARAM_MIX, 1.0f);
  g.chain({delay});
  // The internal transport runs at 120 BPM, also while it stops: a dotted
  // eighth of 0.375 s.
  Channels out = g.renderOffline({impulse(24000), impulse(24000)});
  AUD_CHECK(firstAbove(out[0], 0.5f) == 18000);
  g.tempo(150.0);
  g.renderOffline({silence(48000), silence(48000)});
  out = g.renderOffline({impulse(24000), impulse(24000)});
  AUD_CHECK(firstAbove(out[0], 0.5f) == 14400);
}

// ############################################################################
// RealtimeSanitizer

namespace {

// aud.test.probe: allocates on the audio thread when AUD_TEST_RTSAN_PROBE
// is set; scripts/test-native.js checks that the rtsan run catches it, so
// that the run's silence about the effects means something.
const bool g_probe = std::getenv("AUD_TEST_RTSAN_PROBE") != nullptr;
char* volatile g_probeSink = nullptr;

void* probeCreate(const AudNodeDescriptor*, const AudHostApi*) { return new int(0); }
void probeDestroy(void* instance) { delete static_cast<int*>(instance); }
int32_t probePrepare(void*, const AudPrepareInfo*) { return AUD_OK; }
void probeReset(void*, uint32_t) {}
void probeSetParam(void*, uint32_t, float) {}
void probeProcess(void*, const AudProcessContext* context) {
  if (g_probe) {
    g_probeSink = new char[16];
    delete[] g_probeSink;
  }
  for (uint32_t c = 0; c < context->outputs[0].num_channels; ++c) {
    std::memset(context->outputs[0].channels[c], 0, sizeof(float) * context->frames);
  }
}

const AudBusDescriptor kProbeOutputs[] = {
    {sizeof(AudBusDescriptor), "out", "Output", AUD_BUS_MAIN, 2, 2, 2},
};

const AudNodeVTable kProbeVTable = {
    sizeof(AudNodeVTable), probeCreate, probeDestroy, probePrepare, probeReset,
    probeSetParam,         probeProcess, nullptr,     nullptr,      nullptr,
    nullptr,               nullptr,
};

const AudNodeDescriptor kProbeDescriptor = {
    sizeof(AudNodeDescriptor),
    AUD_ABI_VERSION_MAJOR,
    AUD_ABI_VERSION_MINOR,
    1,
    "aud.test.probe",
    "Probe",
    "Test",
    AUD_NODE_CAP_VARIABLE_BLOCK,
    0,
    0,
    1,
    nullptr,
    kProbeOutputs,
    0,
    0,
    nullptr,
    nullptr,
    0,
    0,
    nullptr,
    nullptr,
    &kProbeVTable,
};

}  // namespace

AUD_TEST(rtsan_probe) {
  Graph g;
  const AudHostApi* host = g.host();
  AUD_CHECK(host->register_node_type(host->host, &kProbeDescriptor) == AUD_OK);
  std::vector<int32_t> nodes;
  for (const std::string& id : typeIds()) nodes.push_back(g.node(id.c_str()));
  g.chain(nodes);
  const int32_t probe = aud_graph_create_node(g.graph, "aud.test.probe", nullptr);
  AUD_CHECK(aud_graph_begin(g.graph) == AUD_OK);
  AUD_CHECK(aud_graph_connect(g.graph, probe, 0, AUD_GRAPH_NODE, 0, 0) == AUD_OK);
  AUD_CHECK(aud_graph_commit(g.graph) > 0);
  const Channels out = g.renderLive({chord(9600, 0.5f), chord(9600, 0.5f)});
  AUD_CHECK(peak(out[0]) > 0.01f);
}

int main() { return aud_test::run(); }
