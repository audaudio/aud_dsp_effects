// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// What the native tests of the effect nodes share: the descriptors of the
// package without an engine, a node driven through its bare vtable in
// blocks of any size, a graph of the engine with the package registered,
// and test signals.

#ifndef AUD_EFFECTS_FIXTURE_HPP
#define AUD_EFFECTS_FIXTURE_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "aud_audio_graph.h"
#include "aud_dsp_effects.h"
#include "aud_test.hpp"

// The realtime calls of the bare vtable carry Clang's nonblocking effect in
// the rtsan build, so that the RealtimeSanitizer checks the nodes outside
// the graph too.
#if defined(__has_feature)
#if __has_feature(realtime_sanitizer)
#define AUD_TEST_REALTIME [[clang::nonblocking]]
#endif
#endif
#ifndef AUD_TEST_REALTIME
#define AUD_TEST_REALTIME
#endif

namespace aud_effects_test {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

using Channels = std::vector<std::vector<float>>;

// The node types of the package.
inline const std::vector<std::string>& typeIds() {
  static const std::vector<std::string> ids = {
      AUD_DSP_EFFECTS_FREEVERB_TYPE_ID,     AUD_DSP_EFFECTS_PLATE_TYPE_ID,
      AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID, AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID,
      AUD_DSP_EFFECTS_LIMITER_TYPE_ID,      AUD_DSP_EFFECTS_TREMOLO_TYPE_ID,
  };
  return ids;
}

// ............................................................................
// Signals

// A deterministic noise from -1 to 1.
struct Noise {
  uint32_t state = 0x12345678u;
  float next() {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / 8388608.0f - 1.0f;
  }
};

inline std::vector<float> silence(size_t frames) {
  return std::vector<float>(frames, 0.0f);
}

inline std::vector<float> impulse(size_t frames, size_t at = 0,
                                  float amplitude = 1.0f) {
  std::vector<float> samples(frames, 0.0f);
  if (at < frames) samples[at] = amplitude;
  return samples;
}

inline std::vector<float> sine(double frequency, size_t frames,
                               float amplitude = 0.5f, double rate = kRate) {
  std::vector<float> samples(frames);
  for (size_t i = 0; i < frames; ++i) {
    samples[i] = amplitude * static_cast<float>(
                                 std::sin(2 * kPi * frequency * i / rate));
  }
  return samples;
}

// A dense chord whose largest sample is `peak`.
inline std::vector<float> chord(size_t frames, float peak = 0.5f,
                                double rate = kRate, double phase = 0.0) {
  const double notes[] = {130.81, 164.81, 196.0, 246.94, 293.66, 369.99, 440.0};
  std::vector<double> raw(frames, 0.0);
  double largest = 0.0;
  for (size_t i = 0; i < frames; ++i) {
    for (size_t n = 0; n < 7; ++n) {
      raw[i] += std::sin(2 * kPi * notes[n] * i / rate + phase * n);
    }
    largest = std::max(largest, std::fabs(raw[i]));
  }
  std::vector<float> samples(frames);
  for (size_t i = 0; i < frames; ++i) {
    samples[i] = static_cast<float>(raw[i] * peak / largest);
  }
  return samples;
}

// ............................................................................
// Checks

inline float peak(const std::vector<float>& samples, size_t from = 0,
                  size_t to = SIZE_MAX) {
  float result = 0;
  for (size_t i = from; i < to && i < samples.size(); ++i) {
    result = std::max(result, std::fabs(samples[i]));
  }
  return result;
}

inline bool allFinite(const Channels& channels) {
  for (const auto& channel : channels) {
    for (float sample : channel) {
      if (!std::isfinite(sample)) return false;
    }
  }
  return true;
}

inline size_t subnormals(const Channels& channels) {
  size_t count = 0;
  for (const auto& channel : channels) {
    for (float sample : channel) {
      if (std::fpclassify(sample) == FP_SUBNORMAL) count += 1;
    }
  }
  return count;
}

inline double largestDifference(const Channels& a, const Channels& b) {
  double result = a.size() == b.size() ? 0.0 : 1e30;
  for (size_t c = 0; c < a.size() && c < b.size(); ++c) {
    if (a[c].size() != b[c].size()) return 1e30;
    for (size_t i = 0; i < a[c].size(); ++i) {
      result = std::max(result, std::fabs(static_cast<double>(a[c][i]) -
                                          static_cast<double>(b[c][i])));
    }
  }
  return result;
}

// The largest step between neighbouring frames: a click is a step far above
// what the slope of the signal allows.
inline float largestStep(const std::vector<float>& samples, size_t from = 1,
                         size_t to = SIZE_MAX) {
  float result = 0;
  for (size_t i = std::max<size_t>(from, 1); i < to && i < samples.size();
       ++i) {
    result = std::max(result, std::fabs(samples[i] - samples[i - 1]));
  }
  return result;
}

// The first frame from `from` whose magnitude exceeds `threshold`, or
// SIZE_MAX.
inline size_t firstAbove(const std::vector<float>& samples, float threshold,
                         size_t from = 0) {
  for (size_t i = from; i < samples.size(); ++i) {
    if (std::fabs(samples[i]) > threshold) return i;
  }
  return SIZE_MAX;
}

// The linear ceiling of `db` as the limiter computes it.
inline float ceilingOf(float db) {
  return static_cast<float>(std::pow(10.0, db / 20.0));
}

// ............................................................................
// The bare vtable

// The node types of the package, registered with a host of their own.
struct Registry {
  const AudNodeDescriptor* types[16] = {};
  uint32_t count = 0;

  static int32_t add(void* host, const AudNodeDescriptor* descriptor) {
    auto* registry = static_cast<Registry*>(host);
    registry->types[registry->count++] = descriptor;
    return AUD_OK;
  }

  static const Registry& get() {
    static const Registry registry = [] {
      Registry r;
      AudHostApi host{};
      host.struct_size = sizeof(AudHostApi);
      host.abi_major = AUD_ABI_VERSION_MAJOR;
      host.abi_minor = AUD_ABI_VERSION_MINOR;
      host.host = &r;
      host.register_node_type = add;
      aud_dsp_effects_register(&host);
      return r;
    }();
    return registry;
  }
};

inline const AudNodeDescriptor* typeOf(const std::string& typeId) {
  const Registry& registry = Registry::get();
  for (uint32_t i = 0; i < registry.count; ++i) {
    if (typeId == registry.types[i]->type_id) return registry.types[i];
  }
  return nullptr;
}

inline int32_t paramIndex(const AudNodeDescriptor* type, const char* id) {
  for (uint32_t i = 0; i < type->num_params; ++i) {
    if (std::strcmp(type->params[i].id, id) == 0) return static_cast<int32_t>(i);
  }
  return -1;
}

// The realtime calls, each in a nonblocking function of its own.
inline void processRealtime(const AudNodeVTable* vtable, void* node,
                            const AudProcessContext* context)
    AUD_TEST_REALTIME {
  vtable->process(node, context);
}

inline void setParamRealtime(const AudNodeVTable* vtable, void* node,
                             uint32_t index, float value) AUD_TEST_REALTIME {
  vtable->set_param(node, index, value);
}

inline void resetRealtime(const AudNodeVTable* vtable, void* node,
                          uint32_t reason) AUD_TEST_REALTIME {
  vtable->reset(node, reason);
}

// An event at an absolute frame of a render.
struct TimedEvent {
  size_t frame;
  AudEvent event;
};

// A node of the package driven through its vtable.
struct Instance {
  const AudNodeDescriptor* type = nullptr;
  void* node = nullptr;
  double rate;
  uint32_t maxFrames;
  uint32_t inputs;
  uint32_t outputs;
  int32_t prepared = AUD_ERROR_STATE;
  // Handed to nodes with AUD_NODE_CAP_TRANSPORT when set.
  const AudTransportSnapshot* transport = nullptr;
  int64_t position = 0;

  // Creates and prepares a node of `typeId`; `outputs` 0 takes the
  // descriptor's default, or the inputs for the limiter.
  explicit Instance(const std::string& typeId, double sampleRate = kRate,
                    uint32_t largest = 256, uint32_t inputChannels = 2,
                    uint32_t outputChannels = 0)
      : rate(sampleRate), maxFrames(largest), inputs(inputChannels) {
    type = typeOf(typeId);
    AUD_CHECK(type != nullptr);
    outputs = outputChannels;
    if (type == nullptr) return;
    if (outputs == 0) {
      outputs = std::min(std::max(inputs, type->output_buses[0].min_channels),
                         type->output_buses[0].max_channels);
    }
    node = type->vtable->create(type, nullptr);
    AUD_CHECK(node != nullptr);
    prepared = prepare(sampleRate, largest);
    AUD_CHECK(prepared == AUD_OK);
  }

  ~Instance() {
    if (node != nullptr) type->vtable->destroy(node);
  }

  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  // Prepares the node again; the rate and the largest block change only
  // when the node takes them.
  int32_t prepare(double sampleRate, uint32_t largest) {
    AudPrepareInfo info{};
    info.struct_size = sizeof(AudPrepareInfo);
    info.max_frames = largest;
    info.sample_rate = sampleRate;
    info.num_input_buses = 1;
    info.input_channels = &inputs;
    info.num_output_buses = 1;
    info.output_channels = &outputs;
    const int32_t result = type->vtable->prepare(node, &info);
    if (result == AUD_OK) {
      rate = sampleRate;
      maxFrames = largest;
    }
    return result;
  }

  int32_t index(const char* id) const { return paramIndex(type, id); }

  void set(const char* id, float value) {
    const int32_t i = index(id);
    AUD_CHECK(i >= 0);
    if (i >= 0) setParamRealtime(type->vtable, node, static_cast<uint32_t>(i), value);
  }

  void reset(uint32_t reason = AUD_RESET_STOP) {
    resetRealtime(type->vtable, node, reason);
  }

  uint32_t latency() const {
    return type->vtable->get_latency == nullptr
               ? 0
               : type->vtable->get_latency(node);
  }

  uint32_t tail() const {
    return type->vtable->get_tail == nullptr ? 0 : type->vtable->get_tail(node);
  }

  // Renders `in` - one vector per input channel - in blocks of the sizes of
  // `blocks`, cycled, with `events` at their frames; in place when
  // `inPlace` and the bus formats allow it. Returns the output channels.
  Channels render(const Channels& in, std::vector<uint32_t> blocks = {},
                  bool inPlace = false,
                  const std::vector<TimedEvent>& events = {}) {
    if (blocks.empty()) blocks.push_back(maxFrames);
    const size_t frames = in.empty() ? 0 : in[0].size();
    Channels out(outputs, std::vector<float>(frames, 0.0f));
    if (node == nullptr || prepared != AUD_OK) return out;
    const bool aliased = inPlace && inputs == outputs;
    std::vector<std::vector<float>> inBlock(inputs,
                                            std::vector<float>(maxFrames));
    std::vector<std::vector<float>> outBlock(outputs,
                                             std::vector<float>(maxFrames));
    std::vector<float*> inPointers;
    std::vector<float*> outPointers;
    for (auto& channel : inBlock) inPointers.push_back(channel.data());
    for (auto& channel : outBlock) outPointers.push_back(channel.data());
    const AudAudioBus inBus{sizeof(AudAudioBus), inputs,
                            aliased ? outPointers.data() : inPointers.data()};
    const AudAudioBus outBus{sizeof(AudAudioBus), outputs, outPointers.data()};
    std::vector<AudEvent> blockEvents;
    blockEvents.reserve(events.size());
    size_t done = 0;
    size_t nextEvent = 0;
    size_t cycle = 0;
    while (done < frames) {
      const uint32_t block = static_cast<uint32_t>(std::min<size_t>(
          std::min(blocks[cycle++ % blocks.size()], maxFrames), frames - done));
      for (uint32_t c = 0; c < inputs; ++c) {
        float* target = aliased ? outBlock[c].data() : inBlock[c].data();
        std::copy(in[c].begin() + static_cast<std::ptrdiff_t>(done),
                  in[c].begin() + static_cast<std::ptrdiff_t>(done + block),
                  target);
      }
      blockEvents.clear();
      while (nextEvent < events.size() &&
             events[nextEvent].frame < done + block) {
        AudEvent event = events[nextEvent].event;
        event.sample_offset = static_cast<uint32_t>(
            events[nextEvent].frame < done ? 0 : events[nextEvent].frame - done);
        blockEvents.push_back(event);
        nextEvent += 1;
      }
      AudProcessContext context{};
      context.struct_size = sizeof(AudProcessContext);
      context.frames = block;
      context.sample_rate = rate;
      context.sample_position = position;
      context.num_input_buses = 1;
      context.inputs = &inBus;
      context.num_output_buses = 1;
      context.outputs = &outBus;
      context.num_events = static_cast<uint32_t>(blockEvents.size());
      context.events = blockEvents.empty() ? nullptr : blockEvents.data();
      context.transport =
          (type->capabilities & AUD_NODE_CAP_TRANSPORT) ? transport : nullptr;
      processRealtime(type->vtable, node, &context);
      for (uint32_t c = 0; c < outputs; ++c) {
        std::copy(outBlock[c].begin(), outBlock[c].begin() + block,
                  out[c].begin() + static_cast<std::ptrdiff_t>(done));
      }
      done += block;
      position += block;
    }
    return out;
  }

  // Renders `frames` frames of the same mono or stereo signal on every
  // input channel.
  Channels renderSignal(const std::vector<float>& signal,
                        std::vector<uint32_t> blocks = {}, bool inPlace = false,
                        const std::vector<TimedEvent>& events = {}) {
    return render(Channels(inputs, signal), std::move(blocks), inPlace, events);
  }
};

// A parameter event of `type` at an absolute frame.
inline TimedEvent paramEvent(const AudNodeDescriptor* type, const char* id,
                             float value, uint32_t ramp, size_t frame) {
  return TimedEvent{frame,
                    aud_event_param(static_cast<uint32_t>(paramIndex(type, id)),
                                    value, ramp, 0)};
}

// A transport snapshot of one segment at `tempo`.
struct Transport {
  AudTransportSegment segment{};
  AudTransportSnapshot snapshot{};

  explicit Transport(double tempo) {
    segment.struct_size = sizeof(AudTransportSegment);
    segment.tempo = tempo;
    segment.time_signature_numerator = 4;
    segment.time_signature_denominator = 4;
    snapshot.struct_size = sizeof(AudTransportSnapshot);
    snapshot.num_segments = 1;
    snapshot.segments = &segment;
  }
};

// ............................................................................
// The graph

// A graph of the engine with one stereo input and one stereo output bus and
// the package registered. The engine's watchdog counts what the audio
// thread must not do; every test ends without a violation.
struct Graph {
  AudGraph* graph = nullptr;
  uint32_t channels = 2;
  uint32_t maxFrames;
  double rate;
  uint64_t violationsBefore = aud_graph_watchdog_violations();

  explicit Graph(uint32_t frames = 256, double sampleRate = kRate)
      : maxFrames(frames), rate(sampleRate) {
    AudGraphConfig config{};
    config.struct_size = sizeof(AudGraphConfig);
    config.sample_rate = sampleRate;
    config.max_frames = frames;
    config.num_input_buses = 1;
    config.input_channels = &channels;
    config.num_output_buses = 1;
    config.output_channels = &channels;
    graph = aud_graph_create(&config);
    AUD_CHECK(graph != nullptr);
    AUD_CHECK(aud_dsp_effects_register(host()) == AUD_OK);
  }

  ~Graph() {
    AUD_CHECK(aud_graph_watchdog_violations() - violationsBefore == 0);
    aud_graph_destroy(graph);
  }

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  const AudHostApi* host() const { return aud_graph_host_api(graph); }

  // A node of `typeId`, not connected; 0 channels take the defaults.
  int32_t node(const char* typeId, uint32_t inputs = 0, uint32_t outputs = 0) {
    AudNodeConfig config{};
    config.struct_size = sizeof(AudNodeConfig);
    config.num_input_buses = inputs == 0 ? 0 : 1;
    config.input_channels = &inputs;
    config.num_output_buses = outputs == 0 ? 0 : 1;
    config.output_channels = &outputs;
    const int32_t handle = aud_graph_create_node(graph, typeId, &config);
    AUD_CHECK(handle > 0);
    return handle;
  }

  // Connects the graph's input through `nodes` to its output and starts.
  void chain(const std::vector<int32_t>& nodes) {
    AUD_CHECK(aud_graph_begin(graph) == AUD_OK);
    int32_t from = AUD_GRAPH_NODE;
    for (int32_t node : nodes) {
      AUD_CHECK(aud_graph_connect(graph, from, 0, node, 0, 0) == AUD_OK);
      from = node;
    }
    AUD_CHECK(aud_graph_connect(graph, from, 0, AUD_GRAPH_NODE, 0, 0) == AUD_OK);
    AUD_CHECK(aud_graph_commit(graph) > 0);
    if (aud_graph_state(graph) != AUD_GRAPH_RUNNING) {
      AUD_CHECK(aud_graph_start(graph) == AUD_OK);
    }
  }

  void param(int32_t node, uint32_t index, float value, uint32_t ramp = 0) {
    AUD_CHECK(aud_graph_set_param(graph, node, index, value, ramp) == AUD_OK);
  }

  void tempo(double bpm) {
    AudTransportRequest request{};
    request.struct_size = sizeof(AudTransportRequest);
    request.type = AUD_TRANSPORT_REQUEST_SET_TEMPO;
    request.at.struct_size = sizeof(AudTimestamp);
    request.value = bpm;
    AUD_CHECK(aud_graph_transport(graph, &request) == AUD_OK);
  }

  // Renders `in` offline in blocks of `block` frames.
  Channels renderOffline(const Channels& in, uint32_t block = 0) {
    const size_t frames = in[0].size();
    Channels input = in;
    Channels out(channels, std::vector<float>(frames, 0.0f));
    std::vector<float*> inPointers;
    std::vector<float*> outPointers;
    for (auto& channel : input) inPointers.push_back(channel.data());
    for (auto& channel : out) outPointers.push_back(channel.data());
    const AudAudioBus inBus{sizeof(AudAudioBus), channels, inPointers.data()};
    const AudAudioBus outBus{sizeof(AudAudioBus), channels,
                             outPointers.data()};
    AudOfflineRequest request{};
    request.struct_size = sizeof(AudOfflineRequest);
    request.frames = static_cast<uint32_t>(frames);
    request.block_frames = block;
    request.inputs = &inBus;
    request.outputs = &outBus;
    request.start_sample_position = aud_graph_sample_position(graph);
    AUD_CHECK(aud_graph_render_offline(graph, &request) == AUD_OK);
    return out;
  }

  // Renders `in` like a stream, block by block on the calling thread.
  Channels renderLive(const Channels& in) {
    const size_t frames = in[0].size();
    Channels out(channels, std::vector<float>(frames, 0.0f));
    Channels inBlock(channels, std::vector<float>(maxFrames));
    Channels outBlock(channels, std::vector<float>(maxFrames));
    std::vector<float*> inPointers;
    std::vector<float*> outPointers;
    for (auto& channel : inBlock) inPointers.push_back(channel.data());
    for (auto& channel : outBlock) outPointers.push_back(channel.data());
    const AudAudioBus inBus{sizeof(AudAudioBus), channels, inPointers.data()};
    const AudAudioBus outBus{sizeof(AudAudioBus), channels,
                             outPointers.data()};
    size_t done = 0;
    while (done < frames) {
      const uint32_t block =
          static_cast<uint32_t>(std::min<size_t>(maxFrames, frames - done));
      for (uint32_t c = 0; c < channels; ++c) {
        std::copy(in[c].begin() + static_cast<std::ptrdiff_t>(done),
                  in[c].begin() + static_cast<std::ptrdiff_t>(done + block),
                  inBlock[c].begin());
      }
      AudRenderRequest request{};
      request.struct_size = sizeof(AudRenderRequest);
      request.frames = block;
      request.num_input_buses = 1;
      request.inputs = &inBus;
      request.num_output_buses = 1;
      request.outputs = &outBus;
      AUD_CHECK(aud_graph_render(graph, &request) == AUD_OK);
      for (uint32_t c = 0; c < channels; ++c) {
        std::copy(outBlock[c].begin(), outBlock[c].begin() + block,
                  out[c].begin() + static_cast<std::ptrdiff_t>(done));
      }
      done += block;
    }
    return out;
  }
};

}  // namespace aud_effects_test

#endif  // AUD_EFFECTS_FIXTURE_HPP
