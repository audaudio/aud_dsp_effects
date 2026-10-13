// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// The bench of the package (ticket 28, scope 5): renders a node through its
// vtable on the calling thread and times every process call. The engine
// times only whole blocks, so this is where the render time of one node
// comes from - in `dart test` and in an integration test on a phone.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "aud_dsp_effects.h"

namespace {

// ############################################################################
// A minimal host

// The node types the package registers with the bench's host.
struct AudBenchRegistry {
  static constexpr uint32_t kCapacity = 16;
  const AudNodeDescriptor* types[kCapacity] = {};
  uint32_t count = 0;
};

int32_t benchRegister(void* host, const AudNodeDescriptor* descriptor) {
  auto* registry = static_cast<AudBenchRegistry*>(host);
  if (registry->count == AudBenchRegistry::kCapacity) {
    return AUD_ERROR_CAPACITY;
  }
  registry->types[registry->count++] = descriptor;
  return AUD_OK;
}

void* benchAlloc(void*, size_t bytes) { return std::malloc(bytes); }

void benchFree(void*, void* memory) { std::free(memory); }

void benchLog(void*, int32_t, const char*) {}

// The host api that registers the node types with `registry`.
AudHostApi benchHost(AudBenchRegistry& registry) {
  AudHostApi host = {};
  host.struct_size = sizeof(AudHostApi);
  host.abi_major = AUD_ABI_VERSION_MAJOR;
  host.abi_minor = AUD_ABI_VERSION_MINOR;
  host.abi_oldest_minor = AUD_ABI_VERSION_MINOR;
  host.host = &registry;
  host.register_node_type = benchRegister;
  host.alloc = benchAlloc;
  host.free = benchFree;
  host.log = benchLog;
  return host;
}

// ############################################################################
// The test signal

// The length of the cycle of the test signal and of its parts in seconds.
constexpr double kCycleSeconds = 4.0;
constexpr double kSustainSeconds = 2.0;
constexpr double kDecaySeconds = 1.0;
constexpr double kDecayTime = 0.15;  // the time constant of the decay

// The peak of the chord: -6 dBFS.
constexpr double kPeak = 0.5011872336272722;

// The notes of the chord in Hz: C3 E3 G3 B3 D4 F#4 A4, each with a second
// and a third harmonic.
constexpr double kChord[] = {130.81, 164.81, 196.00, 246.94,
                             293.66, 369.99, 440.00};

// The chord of channel `c` at `t` seconds, before it is scaled: each note
// with its harmonics, the partials of each channel at phases of their own.
double chordAt(double t, uint32_t c) {
  constexpr double kTwoPi = 6.283185307179586;
  double value = 0.0;
  for (size_t n = 0; n < sizeof(kChord) / sizeof(kChord[0]); ++n) {
    const double phase = 0.7 * static_cast<double>(n * (c + 1));
    for (int h = 1; h <= 3; ++h) {
      value += std::sin(kTwoPi * kChord[n] * h * t + phase * h) / h;
    }
  }
  return value;
}

// One cycle of the test signal per channel: the chord at -6 dBFS, its
// decay and silence; the right channel's partials start at other phases.
std::vector<std::vector<float>> testSignal(double sampleRate,
                                           uint32_t channels) {
  const size_t frames = static_cast<size_t>(kCycleSeconds * sampleRate);
  std::vector<std::vector<float>> signal(channels,
                                         std::vector<float>(frames, 0.0f));
  for (uint32_t c = 0; c < channels; ++c) {
    std::vector<double> raw(frames, 0.0);
    double largest = 0.0;
    for (size_t i = 0; i < frames; ++i) {
      const double t = static_cast<double>(i) / sampleRate;
      const double value = chordAt(t, c);
      double envelope = 0.0;
      if (t < kSustainSeconds) {
        envelope = 1.0;
      } else if (t < kSustainSeconds + kDecaySeconds) {
        envelope = std::exp(-(t - kSustainSeconds) / kDecayTime);
      }
      raw[i] = value * envelope;
      if (t < kSustainSeconds) largest = std::max(largest, std::fabs(value));
    }
    const double scale = largest > 0 ? kPeak / largest : 0.0;
    for (size_t i = 0; i < frames; ++i) {
      signal[c][i] = static_cast<float>(raw[i] * scale);
    }
  }
  return signal;
}

// ############################################################################
// The run

// Whether `request` asks for something the bench can measure.
bool valid(const AudEffectsBenchRequest* request,
           const AudEffectsBenchResult* result) {
  if (request == nullptr || result == nullptr) return false;
  if (request->struct_size < sizeof(AudEffectsBenchRequest)) return false;
  if (result->struct_size < sizeof(AudEffectsBenchResult)) return false;
  if (request->frames == 0 || request->blocks == 0) return false;
  if (!(request->sample_rate >= 8000 && request->sample_rate <= 384000)) {
    return false;
  }
  if (request->type_id == nullptr) return false;
  return request->num_params == 0 ||
         (request->param_ids != nullptr && request->param_values != nullptr);
}

// The descriptor of `typeId` among the package's node types, or null.
const AudNodeDescriptor* typeOf(const AudBenchRegistry& registry,
                                const char* typeId) {
  for (uint32_t i = 0; i < registry.count; ++i) {
    if (std::strcmp(registry.types[i]->type_id, typeId) == 0) {
      return registry.types[i];
    }
  }
  return nullptr;
}

// The index of the parameter `id` of `type`, or -1.
int32_t paramIndexOf(const AudNodeDescriptor& type, const char* id) {
  for (uint32_t i = 0; i < type.num_params; ++i) {
    if (id != nullptr && std::strcmp(type.params[i].id, id) == 0) {
      return static_cast<int32_t>(i);
    }
  }
  return -1;
}

// The mean, the 99th percentile and the maximum of the `durations` of the
// timed blocks of `request`.
void summarize(const AudEffectsBenchRequest& request,
               const std::vector<double>& durations,
               AudEffectsBenchResult& result) {
  double sum = 0.0;
  for (double duration : durations) sum += duration;
  std::vector<double> sorted = durations;
  std::sort(sorted.begin(), sorted.end());
  const size_t p99 = static_cast<size_t>(
      std::ceil(0.99 * static_cast<double>(sorted.size())) - 1);
  result.blocks = request.blocks;
  result.block_ns = 1e9 * request.frames / request.sample_rate;
  result.mean_ns = sum / static_cast<double>(durations.size());
  result.p99_ns = sorted[p99];
  result.max_ns = sorted.back();
}

// Renders and times the blocks of `request` with a prepared node.
void measure(const AudEffectsBenchRequest& request,
             const AudNodeDescriptor& type, void* node, uint32_t inputs,
             uint32_t outputs, AudEffectsBenchResult& result) {
  const uint32_t frames = request.frames;
  const std::vector<std::vector<float>> signal =
      testSignal(request.sample_rate, inputs);
  const size_t cycle = signal[0].size();
  std::vector<std::vector<float>> in(inputs, std::vector<float>(frames));
  std::vector<std::vector<float>> out(outputs, std::vector<float>(frames));
  std::vector<float*> inPointers;
  std::vector<float*> outPointers;
  for (auto& channel : in) inPointers.push_back(channel.data());
  for (auto& channel : out) outPointers.push_back(channel.data());
  const AudAudioBus inBus = {sizeof(AudAudioBus), inputs, inPointers.data()};
  const AudAudioBus outBus = {sizeof(AudAudioBus), outputs,
                              outPointers.data()};
  AudProcessContext context = {};
  context.struct_size = sizeof(AudProcessContext);
  context.frames = frames;
  context.sample_rate = request.sample_rate;
  context.num_input_buses = 1;
  context.inputs = &inBus;
  context.num_output_buses = 1;
  context.outputs = &outBus;
  const uint32_t total = request.warmup_blocks + request.blocks;
  std::vector<double> durations(request.blocks, 0.0);
  size_t position = 0;
  for (uint32_t block = 0; block < total; ++block) {
    for (uint32_t c = 0; c < inputs; ++c) {
      for (uint32_t i = 0; i < frames; ++i) {
        in[c][i] = signal[c][(position + i) % cycle];
      }
    }
    context.sample_position = static_cast<int64_t>(position);
    const auto start = std::chrono::steady_clock::now();
    type.vtable->process(node, &context);
    const auto end = std::chrono::steady_clock::now();
    if (block >= request.warmup_blocks) {
      durations[block - request.warmup_blocks] = static_cast<double>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
              .count());
    }
    position = (position + frames) % cycle;
  }
  summarize(request, durations, result);
}

int32_t run(const AudEffectsBenchRequest& request,
            AudEffectsBenchResult& result) {
  AudBenchRegistry registry;
  const AudHostApi host = benchHost(registry);
  const int32_t registered = aud_dsp_effects_register(&host);
  if (registered != AUD_OK) return registered;
  const AudNodeDescriptor* type = typeOf(registry, request.type_id);
  if (type == nullptr) return AUD_ERROR_UNKNOWN_TYPE;
  const AudBusDescriptor& inputBus = type->input_buses[0];
  const AudBusDescriptor& outputBus = type->output_buses[0];
  const uint32_t inputs = request.channels;
  if (inputs < inputBus.min_channels || inputs > inputBus.max_channels) {
    return AUD_ERROR_INVALID_ARGUMENT;
  }
  // The limiter keeps the channels; the others render stereo.
  const uint32_t outputs = std::min(
      std::max(inputs, outputBus.min_channels), outputBus.max_channels);
  std::vector<int32_t> indices(request.num_params);
  for (uint32_t p = 0; p < request.num_params; ++p) {
    indices[p] = paramIndexOf(*type, request.param_ids[p]);
    if (indices[p] < 0) return AUD_ERROR_NOT_FOUND;
  }
  void* node = type->vtable->create(type, &host);
  if (node == nullptr) return AUD_ERROR_OUT_OF_MEMORY;
  AudPrepareInfo info = {};
  info.struct_size = sizeof(AudPrepareInfo);
  info.max_frames = request.frames;
  info.sample_rate = request.sample_rate;
  info.num_input_buses = 1;
  info.input_channels = &inputs;
  info.num_output_buses = 1;
  info.output_channels = &outputs;
  int32_t prepared = type->vtable->prepare(node, &info);
  if (prepared == AUD_OK) {
    for (uint32_t p = 0; p < request.num_params; ++p) {
      type->vtable->set_param(node, static_cast<uint32_t>(indices[p]),
                              request.param_values[p]);
    }
    try {
      measure(request, *type, node, inputs, outputs, result);
    } catch (...) {
      prepared = AUD_ERROR_OUT_OF_MEMORY;
    }
  }
  type->vtable->destroy(node);
  return prepared;
}

}  // namespace

AUD_EXPORT int32_t aud_dsp_effects_bench_run(
    const AudEffectsBenchRequest* request, AudEffectsBenchResult* result) {
  if (!valid(request, result)) return AUD_ERROR_INVALID_ARGUMENT;
  try {
    return run(*request, *result);
  } catch (...) {
    return AUD_ERROR_OUT_OF_MEMORY;
  }
}
