// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// What the effect nodes of the package share (ticket 28): a base on
// AudNodeBase of aud_audio_core that clamps the parameters to their
// descriptors, allocates in prepare only, reads a mono or a stereo input and
// publishes the tail for the control thread; the vtable of a node type; and
// the small building blocks of the nodes - a sine oscillator without a sine
// per frame, a smoother, the flush of tiny values against denormals.

#ifndef AUD_EFFECTS_NODE_HPP
#define AUD_EFFECTS_NODE_HPP

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "aud_abi.h"
#include "aud_node_base.hpp"
#include "aud_param_ramp.hpp"

namespace aud_effects {

// ############################################################################
// Constants

// The vendor of every node type of the package.
constexpr const char* kVendor = "Audanika";

// The capabilities every node type of the package declares.
constexpr uint32_t kCapabilities = AUD_NODE_CAP_IN_PLACE |
                                   AUD_NODE_CAP_VARIABLE_BLOCK |
                                   AUD_NODE_CAP_EVENTS | AUD_NODE_CAP_TAIL;

// The flags of a ramped and of a stepped parameter.
constexpr uint32_t kRamped = AUD_PARAM_AUTOMATABLE | AUD_PARAM_RAMPED;
constexpr uint32_t kStepped = AUD_PARAM_AUTOMATABLE | AUD_PARAM_STEPPED;
constexpr uint32_t kBoolean = AUD_PARAM_AUTOMATABLE | AUD_PARAM_BOOLEAN;

// The longest tail a node reports, in seconds.
constexpr double kMaxTailSeconds = 30.0;

// The frames of a chunk over which a node holds coefficients it derives
// from a ramped parameter.
constexpr uint32_t kControlFrames = 32;

constexpr double kPi = 3.14159265358979323846;

// The input of the reverbs and the delays: mono or stereo.
constexpr AudBusDescriptor kStereoInputs[] = {
    {sizeof(AudBusDescriptor), "in", "Input", AUD_BUS_MAIN, 1, 2, 2},
};

// The output of the reverbs and the delays: stereo.
constexpr AudBusDescriptor kStereoOutputs[] = {
    {sizeof(AudBusDescriptor), "out", "Output", AUD_BUS_MAIN, 2, 2, 2},
};

// ############################################################################
// Building blocks

// `x`, or 0 when its magnitude is below 1e-15 (-300 dBFS), so that a
// decaying feedback path never reaches the denormal range, where some CPUs
// slow down. A comparison, which compiles to a select: adding and
// subtracting a constant would round tiny values up to its quantum, and a
// loop with a gain above 0.5 would then cycle there for ever.
inline float flush(float x) { return std::fabs(x) < 1e-15f ? 0.0f : x; }

// `value` within `low` and `high`.
inline float clampTo(float value, float low, float high) {
  return value < low ? low : (value > high ? high : value);
}

// `frames` rounded up and capped at kMaxTailSeconds at `sampleRate`.
inline uint32_t tailFrames(double frames, double sampleRate) {
  const double cap = std::ceil(kMaxTailSeconds * sampleRate);
  const double rounded = std::ceil(frames);
  if (!(rounded < cap)) return static_cast<uint32_t>(cap);
  return rounded > 0 ? static_cast<uint32_t>(rounded) : 0;
}

// A sine and a cosine at a frequency: a rotation of a unit vector per frame,
// so that no frame calls the sine; renormalize() keeps its length at 1. The
// same input renders the same output on every platform.
class AudQuadratureOscillator {
 public:
  // Sets the frequency in Hz at `sampleRate`; the phase continues.
  void setFrequency(double frequency, double sampleRate) {
    const double increment = 2.0 * kPi * frequency / sampleRate;
    cosStep_ = std::cos(increment);
    sinStep_ = std::sin(increment);
  }

  // Restarts at phase 0: sine 0, cosine 1.
  void restart() {
    sin_ = 0.0;
    cos_ = 1.0;
  }

  // Advances by one frame.
  void advance() {
    const double s = sin_ * cosStep_ + cos_ * sinStep_;
    const double c = cos_ * cosStep_ - sin_ * sinStep_;
    sin_ = s;
    cos_ = c;
  }

  // Pulls the vector back onto the unit circle; once per block is enough.
  void renormalize() {
    const double k = 1.5 - 0.5 * (sin_ * sin_ + cos_ * cos_);
    sin_ *= k;
    cos_ *= k;
  }

  double sine() const { return sin_; }
  double cosine() const { return cos_; }

 private:
  double sin_ = 0.0;
  double cos_ = 1.0;
  double sinStep_ = 0.0;
  double cosStep_ = 1.0;
};

// A one-pole glide of a value towards a target, e.g. a delay time, so that
// a jump of the target does not click.
class AudSmoother {
 public:
  // Sets the time constant in seconds at `sampleRate`.
  void setTime(double seconds, double sampleRate) {
    coefficient_ = 1.0 - std::exp(-1.0 / (seconds * sampleRate));
  }

  // Jumps to `value`.
  void reset(double value) { value_ = value; }

  // Moves one frame towards `target` and returns the new value.
  double next(double target) {
    const double difference = target - value_;
    value_ = std::fabs(difference) < 1e-9 ? target
                                          : value_ + difference * coefficient_;
    return value_;
  }

  double value() const { return value_; }

 private:
  double value_ = 0.0;
  double coefficient_ = 1.0;
};

// ############################################################################
// The base of a node

// A node of the package: its parameters clamped to the descriptors, the
// input as a pair of channels and the tail published for the control
// thread. A derived node implements allocate(), renderFrames() and,
// optionally, computeTail() and clear().
class AudEffectNode : public AudNodeBase {
 public:
  // Creates a node with the parameters `params`, each at its default.
  AudEffectNode(const AudParamDescriptor* params, uint32_t numParams)
      : AudNodeBase(numParams), params_(params) {
    for (uint32_t i = 0; i < numParams; ++i) {
      AudNodeBase::setParam(i, params[i].default_value);
    }
  }

  // [control] Allocates the buffers of the node for `info` into locals and
  // swaps them in, so that a failed allocation keeps the node as it was;
  // then clears the state.
  int32_t prepare(const AudPrepareInfo& info) override {
    if (!(info.sample_rate > 0) || !std::isfinite(info.sample_rate) ||
        info.max_frames == 0) {
      return AUD_ERROR_INVALID_ARGUMENT;
    }
    const uint32_t inputs =
        info.num_input_buses > 0 && info.input_channels != nullptr
            ? info.input_channels[0]
            : 0;
    const uint32_t outputs =
        info.num_output_buses > 0 && info.output_channels != nullptr
            ? info.output_channels[0]
            : 0;
    try {
      std::vector<float> silence(info.max_frames, 0.0f);
      const int32_t result = allocate(info, inputs, outputs);
      if (result != AUD_OK) return result;
      silence_.swap(silence);
    } catch (...) {
      return AUD_ERROR_OUT_OF_MEMORY;
    }
    const int32_t result = AudNodeBase::prepare(info);
    clear();
    refreshTail();
    return result;
  }

  // [realtime] Clears the state: the buffers ring out no more.
  void reset(uint32_t reason) override {
    (void)reason;
    clear();
  }

  // [realtime] Sets a parameter at once; hides the base's, so that the
  // vtable reaches it and the tail follows.
  void setParam(uint32_t index, float value) {
    AudNodeBase::setParam(index, value);
    refreshTail();
  }

  // [control] The tail the node reported last, in frames.
  uint32_t tail() const override {
    return tail_.load(std::memory_order_relaxed);
  }

 protected:
  // [control] Allocates the buffers for `info` with `inputs` and `outputs`
  // channels on the main buses into locals and swaps them in; may throw.
  virtual int32_t allocate(const AudPrepareInfo& info, uint32_t inputs,
                           uint32_t outputs) = 0;

  // [realtime] Clears the state after prepare and on a reset.
  virtual void clear() {}

  // [realtime] Renders `frames` frames of the main input `in` to the main
  // output `out` from `offset`; `in` and `out` may alias.
  virtual void renderFrames(const AudProcessContext& context,
                            const AudAudioBus& in, const AudAudioBus& out,
                            uint32_t offset, uint32_t frames) = 0;

  // The tail in frames for the targets of the parameters at the prepared
  // sample rate; 0 by default. Runs at the end of every block and when a
  // parameter is set, so it neither allocates nor blocks.
  virtual uint32_t computeTail() const { return 0; }

  // [realtime] Publishes the tail for get_tail on the control thread.
  void refreshTail() {
    tail_.store(sampleRate() > 0 ? computeTail() : 0,
                std::memory_order_relaxed);
  }

  // The value of parameter `index` at frame `i` of the sub-range, within
  // its range; a value that is no number counts as the default.
  float paramAt(uint32_t index, uint32_t i) const {
    const AudParamRamp& r = ramp(index);
    const float value = i < r.remaining()
                            ? r.value() + r.step() * static_cast<float>(i)
                            : r.target();
    return clamped(index, value);
  }

  // The target of parameter `index` within its range.
  float targetOf(uint32_t index) const {
    return clamped(index, ramp(index).target());
  }

  // The target of a stepped parameter as its step.
  int stepOf(uint32_t index) const {
    return static_cast<int>(std::floor(targetOf(index) + 0.5f));
  }

  // Zeros for the prepared largest block: the input of a missing channel.
  const float* silence() const { return silence_.data(); }

  // The pair of input channels from `offset`: the left one twice for a
  // mono input, silence without an input.
  void inputPair(const AudAudioBus& in, uint32_t offset, const float*& left,
                 const float*& right) const {
    if (in.num_channels == 0 || in.channels == nullptr) {
      left = right = silence();
      return;
    }
    left = in.channels[0] + offset;
    right = in.num_channels > 1 ? in.channels[1] + offset : left;
  }

  // `value` clamped to the range of parameter `index`.
  float clamped(uint32_t index, float value) const {
    const AudParamDescriptor& d = params_[index];
    if (std::isnan(value)) return d.default_value;
    return clampTo(value, d.min_value, d.max_value);
  }

  void renderRange(const AudProcessContext& context, uint32_t offset,
                   uint32_t frames) override {
    if (context.num_output_buses < 1 || context.outputs == nullptr) return;
    static const AudAudioBus kNoInput = {sizeof(AudAudioBus), 0, nullptr};
    const AudAudioBus& in = context.num_input_buses > 0 &&
                                    context.inputs != nullptr
                                ? context.inputs[0]
                                : kNoInput;
    renderFrames(context, in, context.outputs[0], offset, frames);
    if (offset + frames >= context.frames) refreshTail();
  }

 private:
  const AudParamDescriptor* params_;
  std::vector<float> silence_;
  std::atomic<uint32_t> tail_{0};
};

// ############################################################################
// The vtable of a node type

// The vtable of a node type T derived from AudEffectNode: the one of
// AudNodeVTableFor<T>, with a create that lets no exception through and
// without the entries of string settings and states, which no node of the
// package has.
template <class T>
AudNodeVTable effectVTable() {
  AudNodeVTable table = AudNodeVTableFor<T>::vtable();
  table.create = [](const AudNodeDescriptor* descriptor,
                    const AudHostApi* host) -> void* {
    try {
      return new (std::nothrow) T(descriptor, host);
    } catch (...) {
      return nullptr;
    }
  };
  table.set_string = nullptr;
  table.save_state = nullptr;
  table.load_state = nullptr;
  return table;
}

// The descriptor of a node type of the package.
inline AudNodeDescriptor effectDescriptor(
    const char* typeId, const char* name, uint32_t capabilities,
    const AudBusDescriptor* inputs, const AudBusDescriptor* outputs,
    uint32_t numParams, const AudParamDescriptor* params,
    const AudNodeVTable* vtable) {
  AudNodeDescriptor d = {};
  d.struct_size = sizeof(AudNodeDescriptor);
  d.abi_major = AUD_ABI_VERSION_MAJOR;
  d.abi_minor = AUD_ABI_VERSION_MINOR;
  d.version = 1;
  d.type_id = typeId;
  d.name = name;
  d.vendor = kVendor;
  d.capabilities = capabilities;
  d.num_input_buses = 1;
  d.num_output_buses = 1;
  d.input_buses = inputs;
  d.output_buses = outputs;
  d.num_params = numParams;
  d.params = params;
  d.vtable = vtable;
  return d;
}

// The descriptors of the node types, defined next to each node.
const AudNodeDescriptor& freeverbDescriptor();
const AudNodeDescriptor& plateDescriptor();
const AudNodeDescriptor& stereoDelayDescriptor();
const AudNodeDescriptor& tempoDelayDescriptor();
const AudNodeDescriptor& limiterDescriptor();
const AudNodeDescriptor& tremoloDescriptor();

}  // namespace aud_effects

#endif  // AUD_EFFECTS_NODE_HPP
