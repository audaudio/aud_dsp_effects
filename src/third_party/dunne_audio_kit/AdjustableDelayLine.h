// Copyright AudioKit. All Rights Reserved.
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm): the
// read position is computed for every push as the write index minus the
// delay, in double precision, instead of a float read index that
// setDelayMs() jumped and push() advanced - a float index loses the
// fraction of the position in a long buffer (2 s at 96 kHz leave 1/64 of a
// frame). setDelaySamples() sets the delay of the next push, so that a
// caller moves the delay smoothly frame by frame; the delay stays between
// one frame and the capacity less one, so that the interpolation reads
// written frames only. push() flushes the value it writes to zero below
// 1e-15 (-300 dBFS), so that a decaying feedback never reaches the denormal
// range.

#pragma once
#include <vector>

namespace DunneCore
{
    class AdjustableDelayLine {
        double sampleRateHz = 0.0;
        double maxDelayMs = 0.0;
        float fbFraction = 0.0f;
        std::vector<float> buffer;
        int writeIndex = 0;
        double delaySamples = 1.0;
        float output = 0.0f;

    public:
        ~AdjustableDelayLine() { deinit(); }

        void init(double sampleRate, double maxDelayMilliseconds);
        void deinit();

        void clear();

        double getMaxDelayMs() { return maxDelayMs; }

        void setDelayMs(double delayMs);
        void setDelaySamples(double samples);
        void setFeedback(float feedback) { fbFraction = feedback; }

        float push(float sample);

        float getOutput() { return output; }
    };

}
