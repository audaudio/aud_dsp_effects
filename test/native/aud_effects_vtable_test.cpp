// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

// The native tests of the effect nodes through their bare vtables (ticket
// 28): the descriptors, prepare, process and reset at 44.1, 48 and 96 kHz in
// blocks of 1 frame up to the largest, in place and out of place, mono
// inputs, parameter ramps, tails, denormals, the limiter's ceiling and
// latency, the delays' echoes and the bench.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "aud_effects_fixture.hpp"

using namespace aud_effects_test;

namespace {

const double kRates[] = {44100.0, 48000.0, 96000.0};

// Blocks from 1 frame to the largest, cycled.
const std::vector<uint32_t> kOddBlocks = {1,  2,   3,   15,  16,  17,  64,
                                          100, 127, 255, 256, 511, 512};

// A stereo test signal: a chord for half the frames, then silence; the
// right channel's partials start at other phases.
Channels stereoSignal(size_t frames, double rate, float level = 0.5f) {
  Channels in = {chord(frames / 2, level, rate), chord(frames / 2, level, rate, 0.9)};
  for (auto& channel : in) channel.resize(frames, 0.0f);
  return in;
}

// Sets every mix to fully wet, so that the tails show.
void wet(Instance& node) {
  if (node.index("mix") >= 0) node.set("mix", 1.0f);
}

}  // namespace

// ############################################################################
// Descriptors

AUD_TEST(declares_six_node_types_on_abi_0_4) {
  const Registry& registry = Registry::get();
  AUD_CHECK(registry.count == 6);
  for (const std::string& id : typeIds()) {
    const AudNodeDescriptor* d = typeOf(id);
    AUD_CHECK(d != nullptr);
    if (d == nullptr) continue;
    AUD_CHECK(d->struct_size == sizeof(AudNodeDescriptor));
    AUD_CHECK(d->abi_major == 0 && d->abi_minor == 4);
    AUD_CHECK(d->version == 1);
    AUD_CHECK(std::string(d->vendor) == "Audanika");
    const uint32_t caps = AUD_NODE_CAP_IN_PLACE | AUD_NODE_CAP_VARIABLE_BLOCK |
                          AUD_NODE_CAP_EVENTS | AUD_NODE_CAP_TAIL;
    AUD_CHECK((d->capabilities & caps) == caps);
    const bool limiter = id == AUD_DSP_EFFECTS_LIMITER_TYPE_ID;
    const bool tempo = id == AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID;
    AUD_CHECK(((d->capabilities & AUD_NODE_CAP_LATENCY) != 0) == limiter);
    AUD_CHECK(((d->capabilities & AUD_NODE_CAP_TRANSPORT) != 0) == tempo);
    AUD_CHECK((d->capabilities & (AUD_NODE_CAP_STRINGS | AUD_NODE_CAP_STATE)) == 0);
    AUD_CHECK(d->num_input_buses == 1 && d->num_output_buses == 1);
    AUD_CHECK(std::string(d->input_buses[0].id) == "in");
    AUD_CHECK(std::string(d->output_buses[0].id) == "out");
    AUD_CHECK(d->input_buses[0].min_channels == 1);
    AUD_CHECK(d->input_buses[0].max_channels == (limiter ? 16u : 2u));
    AUD_CHECK(d->input_buses[0].default_channels == 2);
    AUD_CHECK(d->output_buses[0].min_channels == (limiter ? 1u : 2u));
    AUD_CHECK(d->output_buses[0].max_channels == (limiter ? 16u : 2u));
    for (uint32_t p = 0; p < d->num_params; ++p) {
      const AudParamDescriptor& param = d->params[p];
      AUD_CHECK(param.struct_size == sizeof(AudParamDescriptor));
      AUD_CHECK(param.flags & AUD_PARAM_AUTOMATABLE);
      const bool discrete = (param.flags & (AUD_PARAM_STEPPED | AUD_PARAM_BOOLEAN)) != 0;
      AUD_CHECK(((param.flags & AUD_PARAM_RAMPED) != 0) == !discrete);
      AUD_CHECK(param.min_value <= param.default_value &&
                param.default_value <= param.max_value);
    }
    AUD_CHECK(d->vtable->set_string == nullptr);
    AUD_CHECK(d->vtable->save_state == nullptr && d->vtable->load_state == nullptr);
  }
  AUD_CHECK(typeOf("aud.effects.unknown") == nullptr);
}

AUD_TEST(refuses_a_prepare_without_a_rate_or_a_block) {
  Instance node(AUD_DSP_EFFECTS_PLATE_TYPE_ID);
  AUD_CHECK(node.prepare(0.0, 256) == AUD_ERROR_INVALID_ARGUMENT);
  AUD_CHECK(node.prepare(std::nan(""), 256) == AUD_ERROR_INVALID_ARGUMENT);
  AUD_CHECK(node.prepare(kRate, 0) == AUD_ERROR_INVALID_ARGUMENT);
  // The node keeps what it had and renders on.
  const Channels out = node.renderSignal(impulse(4800));
  AUD_CHECK(allFinite(out));
  AUD_CHECK(peak(out[0]) > 0);
}

// ############################################################################
// Rendering

AUD_TEST(renders_every_node_at_every_rate_and_block_size) {
  for (const std::string& id : typeIds()) {
    for (double rate : kRates) {
      const size_t frames = static_cast<size_t>(rate);
      const Channels in = stereoSignal(frames, rate);
      Instance whole(id, rate, 512);
      Instance odd(id, rate, 512);
      const Channels reference = whole.render(in, {512});
      const Channels rendered = odd.render(in, kOddBlocks);
      AUD_CHECK(allFinite(rendered));
      AUD_CHECK(peak(rendered[0]) > 0.01f && peak(rendered[0]) < 2.0f);
      const double difference = largestDifference(reference, rendered);
      if (difference > 1e-5) {
        std::printf("  %s at %.0f Hz differs by %g between block sizes\n",
                    id.c_str(), rate, difference);
      }
      AUD_CHECK(difference <= 1e-5);
    }
  }
}

AUD_TEST(renders_in_place_like_out_of_place) {
  for (const std::string& id : typeIds()) {
    const Channels in = stereoSignal(24000, kRate);
    Instance separate(id);
    Instance aliased(id);
    const Channels a = separate.render(in, {100, 256, 7});
    const Channels b = aliased.render(in, {100, 256, 7}, true);
    AUD_CHECK(largestDifference(a, b) == 0.0);
  }
}

AUD_TEST(takes_a_mono_input_as_the_same_signal_on_both_sides) {
  for (const std::string& id : typeIds()) {
    const std::vector<float> signal = chord(24000, 0.5f);
    Instance mono(id, kRate, 256, 1, 2);
    Instance stereo(id, kRate, 256, 2, 2);
    const Channels a = mono.render({signal});
    const Channels b = stereo.render({signal, signal});
    AUD_CHECK(a.size() == 2);
    AUD_CHECK(peak(a[1]) > 0.01f);
    AUD_CHECK(largestDifference(a, b) == 0.0);
  }
}

AUD_TEST(renders_silence_without_an_input_bus) {
  for (const std::string& id : typeIds()) {
    Instance node(id);
    std::vector<float> left(256, 1.0f);
    std::vector<float> right(256, 1.0f);
    float* pointers[] = {left.data(), right.data()};
    const AudAudioBus out{sizeof(AudAudioBus), 2, pointers};
    AudProcessContext context{};
    context.struct_size = sizeof(AudProcessContext);
    context.frames = 256;
    context.sample_rate = kRate;
    context.num_output_buses = 1;
    context.outputs = &out;
    processRealtime(node.type->vtable, node.node, &context);
    AUD_CHECK(peak(left) == 0.0f && peak(right) == 0.0f);
  }
}

AUD_TEST(ramps_every_parameter_without_a_click) {
  for (const std::string& id : typeIds()) {
    Instance node(id);
    const AudNodeDescriptor* d = node.type;
    std::vector<TimedEvent> events;
    for (uint32_t p = 0; p < d->num_params; ++p) {
      const AudParamDescriptor& param = d->params[p];
      if ((param.flags & AUD_PARAM_RAMPED) == 0) continue;
      const float low = param.min_value + 0.2f * (param.max_value - param.min_value);
      const float high = param.min_value + 0.8f * (param.max_value - param.min_value);
      // Odd offsets, so that the events split blocks.
      events.push_back(paramEvent(d, param.id, high, 4800, 10003 + p));
      events.push_back(paramEvent(d, param.id, low, 9600, 30011 + p));
    }
    const std::vector<float> signal = sine(220.0, 48000, 0.25f);
    const Channels out = node.renderSignal(signal, {256, 33}, false, events);
    AUD_CHECK(allFinite(out));
    // A sine of 220 Hz at 0.25 steps at most 0.0072 per frame; the effects
    // add their own, a click would add far more.
    for (const auto& channel : out) {
      const float step = largestStep(channel);
      if (step >= 0.2f) std::printf("  %s steps by %g\n", id.c_str(), step);
      AUD_CHECK(step < 0.2f);
    }
  }
}

AUD_TEST(clamps_values_out_of_range_and_no_numbers) {
  for (const std::string& id : typeIds()) {
    for (float value : {-1e9f, 1e9f, std::nanf(""), INFINITY, -INFINITY}) {
      Instance node(id);
      for (uint32_t p = 0; p < node.type->num_params; ++p) {
        node.set(node.type->params[p].id, value);
      }
      const Channels out = node.renderSignal(chord(9600, 0.5f));
      AUD_CHECK(allFinite(out));
      AUD_CHECK(peak(out[0]) < 4.0f);
      AUD_CHECK(node.tail() > 0 || id == AUD_DSP_EFFECTS_TREMOLO_TYPE_ID);
    }
  }
}

AUD_TEST(resets_to_silence) {
  for (const std::string& id : typeIds()) {
    Instance node(id);
    wet(node);
    node.renderSignal(chord(9600, 0.5f));
    node.reset();
    const Channels out = node.renderSignal(silence(9600));
    AUD_CHECK(peak(out[0]) == 0.0f && peak(out[1]) == 0.0f);
  }
}

AUD_TEST(prepares_again_for_another_rate) {
  for (const std::string& id : typeIds()) {
    Instance node(id, 44100.0, 128);
    wet(node);
    node.renderSignal(chord(4410, 0.5f, 44100.0));
    AUD_CHECK(node.prepare(96000.0, 1024) == AUD_OK);
    // Prepare clears the state: silence stays silent.
    const Channels quiet = node.renderSignal(silence(9600), {1024});
    AUD_CHECK(peak(quiet[0]) == 0.0f);
    const Channels out = node.renderSignal(chord(96000, 0.5f, 96000.0), {1024});
    if (!allFinite(out) || !(peak(out[0]) > 0)) std::printf("  %s silent\n", id.c_str());
    AUD_CHECK(allFinite(out) && peak(out[0]) > 0);
  }
}

// ############################################################################
// Tails and denormals

AUD_TEST(rings_out_within_its_tail) {
  for (const std::string& id : typeIds()) {
    Instance node(id);
    wet(node);
    const uint32_t tail = node.tail();
    AUD_CHECK(tail < static_cast<uint32_t>(30 * kRate) + 1);
    const size_t frames = tail + 48000;
    const Channels out = node.renderSignal(impulse(frames, 0, 0.5f));
    for (const auto& channel : out) {
      const float all = peak(channel);
      const float after = peak(channel, tail + 1);
      if (after > 1e-3f * all) {
        std::printf("  %s: %g after a tail of %u frames, %g in all\n", id.c_str(),
                    after, tail, all);
      }
      AUD_CHECK(after <= 1e-3f * all);
    }
  }
}

AUD_TEST(reports_the_tail_of_its_parameters) {
  Instance freeverb(AUD_DSP_EFFECTS_FREEVERB_TYPE_ID);
  const uint32_t room = freeverb.tail();
  freeverb.set("room_size", 1.0f);
  AUD_CHECK(freeverb.tail() > room);
  // Feedback 0.98, 342 passes of the longest comb of 1785 frames.
  AUD_CHECK_NEAR(freeverb.tail(), 342.0 * 1785 + 1702, 2000.0);

  Instance plate(AUD_DSP_EFFECTS_PLATE_TYPE_ID);
  const uint32_t plateTail = plate.tail();
  // Decay 0.5: 2.49 trips of 0.725 s around the figure eight, one more
  // trip and the input.
  AUD_CHECK_NEAR(plateTail / kRate, 3.49 * 0.7254 + 0.02 + 0.03, 0.05);
  plate.set("decay", 0.99f);
  AUD_CHECK(plate.tail() == static_cast<uint32_t>(30 * kRate));

  Instance delay(AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID);
  // 0.4 s times 1 + ceil(ln 0.001 / ln 0.3) = 7; 0.4 as a float is a
  // little more.
  AUD_CHECK_NEAR(delay.tail(), 0.4 * 7 * kRate, 1.0);
  delay.set("ping_pong", 1.0f);
  AUD_CHECK_NEAR(delay.tail(), 0.8 * 7 * kRate, 1.0);
  delay.set("feedback", 0.0f);
  AUD_CHECK_NEAR(delay.tail(), 0.8 * kRate, 1.0);
  delay.set("feedback", 0.999f);
  AUD_CHECK(delay.tail() == AUD_TAIL_INFINITE);

  Instance tempo(AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID);
  // A dotted eighth at 120 BPM: 0.375 s.
  AUD_CHECK(tempo.tail() == static_cast<uint32_t>(0.375 * 7 * kRate));

  Instance limiter(AUD_DSP_EFFECTS_LIMITER_TYPE_ID);
  AUD_CHECK(limiter.tail() == 72 && limiter.latency() == 72);
  Instance tremolo(AUD_DSP_EFFECTS_TREMOLO_TYPE_ID);
  AUD_CHECK(tremolo.tail() == 0 && tremolo.latency() == 0);
}

AUD_TEST(decays_to_zero_without_denormals) {
  for (const std::string& id : typeIds()) {
    Instance node(id);
    wet(node);
    const size_t frames = static_cast<size_t>(30 * kRate);
    std::vector<float> signal = chord(frames, 0.5f);
    std::fill(signal.begin() + 4800, signal.end(), 0.0f);
    // Times the blocks while the effect rings and when it has decayed: a
    // CPU that slows down on denormals would take far longer then.
    const Channels in(2, signal);
    Channels out(2, std::vector<float>(frames));
    std::vector<double> ringing;
    std::vector<double> decayed;
    for (size_t at = 0; at < frames; at += 256) {
      Channels block = {{in[0].begin() + static_cast<std::ptrdiff_t>(at),
                         in[0].begin() + static_cast<std::ptrdiff_t>(at + 256)},
                        {in[1].begin() + static_cast<std::ptrdiff_t>(at),
                         in[1].begin() + static_cast<std::ptrdiff_t>(at + 256)}};
      const auto start = std::chrono::steady_clock::now();
      const Channels rendered = node.render(block);
      const double ns = std::chrono::duration<double, std::nano>(
                            std::chrono::steady_clock::now() - start)
                            .count();
      if (at >= 9600 && at < 9600 + 256 * 200) ringing.push_back(ns);
      if (at >= frames - 256 * 200) decayed.push_back(ns);
      for (int c = 0; c < 2; ++c) {
        std::copy(rendered[c].begin(), rendered[c].end(),
                  out[c].begin() + static_cast<std::ptrdiff_t>(at));
      }
    }
    AUD_CHECK(subnormals(out) == 0);
    // The last second is exact silence.
    const size_t lastSecond = frames - static_cast<size_t>(kRate);
    if (peak(out[0], lastSecond) != 0.0f) {
      std::printf("  %s: %g in the last second, first above 0 at %zu\n", id.c_str(),
                  peak(out[0], lastSecond), firstAbove(out[0], 0.0f, lastSecond));
    }
    AUD_CHECK(peak(out[0], lastSecond) == 0.0f && peak(out[1], lastSecond) == 0.0f);
    std::sort(ringing.begin(), ringing.end());
    std::sort(decayed.begin(), decayed.end());
    const double ringingMedian = ringing[ringing.size() / 2];
    const double decayedMedian = decayed[decayed.size() / 2];
    if (decayedMedian > 3 * ringingMedian + 20000) {
      std::printf("  %s: %g ns per decayed block, %g ns ringing\n", id.c_str(),
                  decayedMedian, ringingMedian);
    }
    AUD_CHECK(decayedMedian <= 3 * ringingMedian + 20000);
  }
}

// ############################################################################
// Reverbs

AUD_TEST(reverbs_answer_an_impulse_in_stereo) {
  for (const char* id : {AUD_DSP_EFFECTS_FREEVERB_TYPE_ID, AUD_DSP_EFFECTS_PLATE_TYPE_ID}) {
    Instance node(id);
    node.set("mix", 1.0f);
    const Channels out = node.renderSignal(impulse(48000));
    // Fully wet: no dry impulse at frame 0, a reverb after it.
    AUD_CHECK(std::fabs(out[0][0]) < 0.5f);
    AUD_CHECK(peak(out[0], 4800, 24000) > 1e-3f);
    AUD_CHECK(peak(out[1], 4800, 24000) > 1e-3f);
    // The sides differ: the reverb is stereo.
    AUD_CHECK(largestDifference({out[0]}, {out[1]}) > 1e-3);
  }
}

AUD_TEST(reverbs_pass_the_dry_signal_at_mix_0) {
  for (const char* id : {AUD_DSP_EFFECTS_FREEVERB_TYPE_ID, AUD_DSP_EFFECTS_PLATE_TYPE_ID,
                         AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID,
                         AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID}) {
    Instance node(id);
    node.set("mix", 0.0f);
    const Channels in = stereoSignal(24000, kRate);
    const Channels out = node.render(in);
    AUD_CHECK(largestDifference(in, out) == 0.0);
  }
}

AUD_TEST(plate_delays_its_reverb_by_the_pre_delay) {
  Instance node(AUD_DSP_EFFECTS_PLATE_TYPE_ID);
  node.set("mix", 1.0f);
  node.set("pre_delay", 0.1f);
  const Channels out = node.renderSignal(impulse(24000));
  // The diffusers and the first taps follow the pre-delay of 4800 frames.
  AUD_CHECK(peak(out[0], 0, 4800) == 0.0f && peak(out[1], 0, 4800) == 0.0f);
  AUD_CHECK(peak(out[0], 4800, 24000) > 1e-3f);
}

// ############################################################################
// Delays

AUD_TEST(stereo_delay_echoes_after_its_time) {
  Instance node(AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID);
  node.set("time", 0.25f);
  node.set("feedback", 0.5f);
  node.set("mix", 1.0f);
  const Channels out = node.renderSignal(impulse(48000));
  for (const auto& channel : out) {
    AUD_CHECK(channel[0] == 0.0f);
    AUD_CHECK_NEAR(channel[12000], 1.0, 1e-6);
    AUD_CHECK_NEAR(channel[24000], 0.5, 1e-6);
    AUD_CHECK_NEAR(channel[36000], 0.25, 1e-6);
    AUD_CHECK(peak(channel, 1, 12000) == 0.0f);
    AUD_CHECK(peak(channel, 12001, 24000) == 0.0f);
  }
}

AUD_TEST(stereo_delay_mix_crossfades_from_dry_to_wet) {
  // DunneAudioKit's mix was inverted, (1 - mix) * wet + mix * dry; the port
  // renders (1 - mix) * dry + mix * wet.
  Instance node(AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID);
  node.set("time", 0.125f);
  node.set("feedback", 0.0f);
  node.set("mix", 0.25f);
  const Channels out = node.renderSignal(impulse(9600));
  AUD_CHECK_NEAR(out[0][0], 0.75, 1e-6);
  AUD_CHECK_NEAR(out[0][6000], 0.25, 1e-6);
}

AUD_TEST(stereo_delay_alternates_the_sides_in_ping_pong) {
  Instance node(AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID);
  node.set("time", 0.125f);
  node.set("feedback", 0.5f);
  node.set("mix", 1.0f);
  node.set("ping_pong", 1.0f);
  const Channels out = node.render({impulse(30000), impulse(30000)});
  const std::vector<float>& left = out[0];
  const std::vector<float>& right = out[1];
  AUD_CHECK_NEAR(left[6000], 1.0, 1e-6);
  AUD_CHECK(peak(right, 0, 11000) == 0.0f);
  AUD_CHECK_NEAR(right[12000], 1.0, 1e-6);
  AUD_CHECK(peak(left, 6001, 17000) == 0.0f);
  // The echo crosses back with the feedback - one frame later, the frame
  // the cross feedback waits for the right line's output.
  AUD_CHECK_NEAR(peak(left, 17999, 18003), 0.5, 1e-6);
  AUD_CHECK_NEAR(peak(right, 23999, 24003), 0.5, 1e-6);
}

AUD_TEST(stereo_delay_glides_to_a_new_time) {
  Instance node(AUD_DSP_EFFECTS_STEREO_DELAY_TYPE_ID);
  node.set("time", 0.25f);
  node.set("feedback", 0.0f);
  node.set("mix", 1.0f);
  const std::vector<float> tone = sine(220.0, 48000, 0.25f);
  const Channels before = node.renderSignal(tone);
  node.set("time", 0.125f);
  const Channels during = node.renderSignal(tone);
  // A jump of the time moves the echo like a tape: no click.
  AUD_CHECK(largestStep(during[0]) < 0.05f);
  node.renderSignal(silence(48000));
  const Channels after = node.renderSignal(impulse(12000));
  AUD_CHECK(firstAbove(after[0], 0.5f) == 6000);
}

AUD_TEST(tempo_delay_follows_the_tempo_of_the_transport) {
  Instance node(AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID);
  node.set("feedback", 0.0f);
  node.set("mix", 1.0f);
  // Without a transport: 120 BPM, a dotted eighth of 0.375 s.
  Channels out = node.renderSignal(impulse(24000));
  AUD_CHECK(firstAbove(out[0], 0.5f) == 18000);
  Transport at100(100.0);
  node.transport = &at100.snapshot;
  node.renderSignal(silence(48000));
  out = node.renderSignal(impulse(24000));
  AUD_CHECK(firstAbove(out[0], 0.5f) == 21600);
  // A quarter note at 100 BPM.
  node.set("division", AUD_TEMPO_DELAY_DIVISION_4);
  node.renderSignal(silence(48000));
  out = node.renderSignal(impulse(48000));
  AUD_CHECK(firstAbove(out[0], 0.5f) == 28800);
  // No tempo: 120 BPM again.
  Transport none(0.0);
  node.transport = &none.snapshot;
  node.renderSignal(silence(48000));
  out = node.renderSignal(impulse(48000));
  AUD_CHECK(firstAbove(out[0], 0.5f) == 24000);
}

AUD_TEST(tempo_delay_glides_when_the_tempo_changes) {
  Instance node(AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID);
  node.set("feedback", 0.0f);
  node.set("mix", 1.0f);
  Transport transport(120.0);
  node.transport = &transport.snapshot;
  const std::vector<float> tone = sine(220.0, 48000, 0.25f);
  node.renderSignal(tone);
  transport.segment.tempo = 90.0;
  const Channels during = node.renderSignal(tone);
  AUD_CHECK(largestStep(during[0]) < 0.05f);
}

AUD_TEST(tempo_delay_stays_within_its_buffer) {
  Instance node(AUD_DSP_EFFECTS_TEMPO_DELAY_TYPE_ID);
  node.set("feedback", 0.0f);
  node.set("mix", 1.0f);
  node.set("division", AUD_TEMPO_DELAY_DIVISION_1);
  Transport slow(30.0);  // a whole note of 8 s
  node.transport = &slow.snapshot;
  const Channels out = node.renderSignal(impulse(5 * 48000));
  AUD_CHECK(firstAbove(out[0], 0.5f) == 4 * 48000);
  AUD_CHECK(node.tail() == static_cast<uint32_t>(4 * kRate));
}

// ############################################################################
// Limiter

AUD_TEST(limiter_delays_an_impulse_by_its_latency) {
  for (double rate : kRates) {
    Instance node(AUD_DSP_EFFECTS_LIMITER_TYPE_ID, rate);
    const uint32_t latency = node.latency();
    AUD_CHECK(latency == static_cast<uint32_t>(std::floor(0.0015 * rate + 0.5)));
    AUD_CHECK(node.tail() == latency);
    const Channels out = node.renderSignal(impulse(4800, 100, 0.5f), kOddBlocks);
    for (const auto& channel : out) {
      AUD_CHECK(channel[100 + latency] == 0.5f);
      AUD_CHECK(firstAbove(channel, 0.0f) == 100 + latency);
      AUD_CHECK(firstAbove(channel, 0.0f, 101 + latency) == SIZE_MAX);
    }
  }
}

AUD_TEST(limiter_keeps_every_sample_below_the_ceiling) {
  for (double rate : kRates) {
    const size_t frames = static_cast<size_t>(rate);
    Noise noise;
    // Noise, a chord and single-sample spikes, 24 dB over full scale.
    std::vector<float> hot = chord(frames, 15.85f, rate);
    std::vector<float> random(frames);
    for (float& sample : random) sample = 15.85f * noise.next();
    std::vector<float> spikes(frames, 0.0f);
    std::vector<float> spikesRight(frames, 0.0f);
    for (size_t i = 0; i < frames; ++i) {
      spikes[i] = 0.1f * std::sin(static_cast<float>(i) * 0.01f);
      spikesRight[i] = spikes[i];
    }
    for (size_t i = 97; i < frames; i += 1 + (noise.state >> 20) % 400) {
      noise.next();
      ((noise.state >> 9) & 1 ? spikes : spikesRight)[i] =
          (noise.state >> 10) & 1 ? 15.85f : -15.85f;
      if ((noise.state >> 11) & 1 && i + 1 < frames) spikes[i + 1] = 15.85f;
    }
    // Numbers that are none.
    std::vector<float> broken = random;
    for (size_t i = 1000; i < frames; i += 1777) {
      broken[i] = (i / 1777) % 3 == 0 ? std::nanf("")
                                      : ((i / 1777) % 3 == 1 ? INFINITY : -INFINITY);
    }
    const std::vector<Channels> inputs = {
        {hot, hot}, {random, chord(frames, 1.0f, rate)}, {spikes, spikesRight}, {broken, broken}};
    for (float ceilingDb : {-1.0f, -6.0f, -0.1f, -24.0f}) {
      for (float gainDb : {0.0f, 24.0f}) {
        const float ceiling = ceilingOf(ceilingDb);
        for (const Channels& in : inputs) {
          Instance node(AUD_DSP_EFFECTS_LIMITER_TYPE_ID, rate, 512);
          node.set("ceiling", ceilingDb);
          node.set("gain", gainDb);
          node.set("release", 0.05f);
          const Channels out = node.render(in, kOddBlocks);
          AUD_CHECK(allFinite(out));
          float loudest = 0.0f;
          for (const auto& channel : out) {
            const float largest = peak(channel);
            if (largest > ceiling) {
              std::printf("  %g over a ceiling of %g at %.0f Hz\n", largest, ceiling,
                          rate);
            }
            AUD_CHECK(largest <= ceiling);
            loudest = std::max(loudest, largest);
          }
          // The limiter limits; it does not mute. The gain is linked, so a
          // quiet channel next to a loud one may stay far below.
          AUD_CHECK(loudest > 0.25f * ceiling);
        }
      }
    }
  }
}

AUD_TEST(limiter_keeps_the_ceiling_while_it_moves) {
  Instance node(AUD_DSP_EFFECTS_LIMITER_TYPE_ID);
  const AudNodeDescriptor* d = node.type;
  const std::vector<TimedEvent> events = {
      paramEvent(d, "ceiling", -12.0f, 4800, 4800),
      paramEvent(d, "gain", 24.0f, 2400, 7000),
      paramEvent(d, "ceiling", 0.0f, 0, 20000),
      paramEvent(d, "release", 0.001f, 100, 25000),
  };
  const Channels out = node.renderSignal(chord(48000, 2.0f), {64, 13, 256}, false, events);
  AUD_CHECK(peak(out[0], 0, 4800) <= ceilingOf(-1.0f));
  AUD_CHECK(peak(out[0], 9600, 20000) <= ceilingOf(-12.0f));
  AUD_CHECK(peak(out[0], 20000) <= 1.0f);
  AUD_CHECK(peak(out[0], 20000) > 0.9f);
}

AUD_TEST(limiter_links_the_channels) {
  Instance node(AUD_DSP_EFFECTS_LIMITER_TYPE_ID);
  const uint32_t latency = node.latency();
  std::vector<float> left(9600, 0.1f);
  std::vector<float> right(9600, 0.1f);
  left[4800] = 8.0f;
  const Channels out = node.render({left, right});
  // The spike on the left lowers the right side by the same gain.
  const float gain = out[0][4800 + latency] / 8.0f;
  AUD_CHECK(gain < 0.12f);
  AUD_CHECK_NEAR(out[1][4800 + latency], 0.1 * gain, 1e-6);
  // Before the reduction the right side passes.
  AUD_CHECK_NEAR(out[1][4000 + latency], 0.1, 1e-6);
}

AUD_TEST(limiter_recovers_after_its_release) {
  Instance node(AUD_DSP_EFFECTS_LIMITER_TYPE_ID);
  node.set("release", 0.01f);
  std::vector<float> signal(48000, 0.5f);
  signal[1000] = 10.0f;
  const Channels out = node.renderSignal(signal);
  const uint32_t latency = node.latency();
  AUD_CHECK(out[0][1000 + latency] < 1.0f);
  // Ten time constants later the gain is back.
  AUD_CHECK_NEAR(out[0][1000 + latency + 4800 + 72], 0.5, 0.001);
}

AUD_TEST(limiter_feeds_a_mono_input_to_every_channel) {
  Instance node(AUD_DSP_EFFECTS_LIMITER_TYPE_ID, kRate, 256, 1, 4);
  const Channels out = node.render({chord(9600, 4.0f)});
  AUD_CHECK(out.size() == 4);
  for (const auto& channel : out) {
    AUD_CHECK(largestDifference({channel}, {out[0]}) == 0.0);
    AUD_CHECK(peak(channel) <= ceilingOf(-1.0f));
  }
  Instance many(AUD_DSP_EFFECTS_LIMITER_TYPE_ID, kRate, 256, 16, 16);
  const Channels sixteen = many.render(Channels(16, chord(4800, 4.0f)));
  for (const auto& channel : sixteen) {
    AUD_CHECK(peak(channel) <= ceilingOf(-1.0f));
  }
}

// ############################################################################
// Tremolo

AUD_TEST(tremolo_modulates_the_gain) {
  Instance node(AUD_DSP_EFFECTS_TREMOLO_TYPE_ID);
  node.set("rate", 2.0f);
  node.set("depth", 1.0f);
  const Channels out = node.renderSignal(std::vector<float>(48000, 1.0f));
  // 1 at the peak of the LFO, 0 at its trough, two cycles a second.
  AUD_CHECK_NEAR(peak(out[0]), 1.0, 1e-4);
  float lowest = 1.0f;
  for (float sample : out[0]) lowest = std::min(lowest, sample);
  AUD_CHECK(lowest < 1e-4f);
  AUD_CHECK_NEAR(out[0][6000], 1.0, 1e-4);   // a quarter cycle: the peak
  AUD_CHECK_NEAR(out[0][18000], 0.0, 1e-4);  // three quarters: the trough
  AUD_CHECK_NEAR(out[0][30000], 1.0, 1e-4);  // the next peak
}

// ############################################################################
// Bench

AUD_TEST(bench_times_every_node) {
  for (const std::string& id : typeIds()) {
    AudEffectsBenchRequest request{};
    request.struct_size = sizeof(AudEffectsBenchRequest);
    request.frames = 64;
    request.blocks = 20;
    request.warmup_blocks = 2;
    request.channels = 2;
    request.sample_rate = kRate;
    request.type_id = id.c_str();
    AudEffectsBenchResult result{};
    result.struct_size = sizeof(AudEffectsBenchResult);
    AUD_CHECK(aud_dsp_effects_bench_run(&request, &result) == AUD_OK);
    AUD_CHECK(result.blocks == 20);
    AUD_CHECK_NEAR(result.block_ns, 64 / kRate * 1e9, 1e-6);
    AUD_CHECK(result.mean_ns > 0 && result.mean_ns <= result.max_ns);
    AUD_CHECK(result.p99_ns <= result.max_ns);
  }
}

AUD_TEST(bench_refuses_what_it_cannot_measure) {
  AudEffectsBenchRequest request{};
  request.struct_size = sizeof(AudEffectsBenchRequest);
  request.frames = 64;
  request.blocks = 2;
  request.channels = 2;
  request.sample_rate = kRate;
  request.type_id = AUD_DSP_EFFECTS_PLATE_TYPE_ID;
  AudEffectsBenchResult result{};
  result.struct_size = sizeof(AudEffectsBenchResult);
  AUD_CHECK(aud_dsp_effects_bench_run(nullptr, &result) == AUD_ERROR_INVALID_ARGUMENT);
  AUD_CHECK(aud_dsp_effects_bench_run(&request, nullptr) == AUD_ERROR_INVALID_ARGUMENT);
  AudEffectsBenchRequest bad = request;
  bad.frames = 0;
  AUD_CHECK(aud_dsp_effects_bench_run(&bad, &result) == AUD_ERROR_INVALID_ARGUMENT);
  bad = request;
  bad.sample_rate = 1000;
  AUD_CHECK(aud_dsp_effects_bench_run(&bad, &result) == AUD_ERROR_INVALID_ARGUMENT);
  bad = request;
  bad.channels = 3;
  AUD_CHECK(aud_dsp_effects_bench_run(&bad, &result) == AUD_ERROR_INVALID_ARGUMENT);
  bad = request;
  bad.type_id = "aud.effects.unknown";
  AUD_CHECK(aud_dsp_effects_bench_run(&bad, &result) == AUD_ERROR_UNKNOWN_TYPE);
  const char* ids[] = {"decay", "colour"};
  const float values[] = {0.6f, 1.0f};
  bad = request;
  bad.num_params = 2;
  bad.param_ids = ids;
  bad.param_values = values;
  AUD_CHECK(aud_dsp_effects_bench_run(&bad, &result) == AUD_ERROR_NOT_FOUND);
  bad.num_params = 1;
  AUD_CHECK(aud_dsp_effects_bench_run(&bad, &result) == AUD_OK);
  bad = request;
  bad.type_id = AUD_DSP_EFFECTS_LIMITER_TYPE_ID;
  bad.channels = 16;
  AUD_CHECK(aud_dsp_effects_bench_run(&bad, &result) == AUD_OK);
}
