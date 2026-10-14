// Copyright AudioKit. All Rights Reserved.
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm): see
// AdjustableDelayLine.h - push() reads at the write index minus the delay
// in double precision, setDelaySamples() moves the delay, the delay is
// clamped to the buffer, the written value is flushed to zero below 1e-15
// and clear() also clears the last output; <cmath> for std::fabs.

#include "AdjustableDelayLine.h"
#include <algorithm>
#include <cmath>

namespace DunneCore
{
    namespace
    {
        // Zero for magnitudes below 1e-15 (-300 dBFS): a comparison, which
        // never rounds a tiny value up and so cannot keep a feedback cycling.
        inline float flushTiny(float x)
        {
            return std::fabs(x) < 1e-15f ? 0.0f : x;
        }
    }

    void AdjustableDelayLine::init(double sampleRate, double maxDelayMilliseconds)
    {
        sampleRateHz = sampleRate;
        maxDelayMs = maxDelayMilliseconds;

        buffer.resize(int(maxDelayMs * sampleRateHz / 1000.0));
        clear();
        writeIndex = 0;
        delaySamples = 1.0;
        fbFraction = 0.0f;
        output = 0.0f;
    }

    void AdjustableDelayLine::deinit()
    {
        buffer.clear();
    }

    void AdjustableDelayLine::clear()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        output = 0.0f;
    }

    void AdjustableDelayLine::setDelayMs(double delayMs)
    {
        if (delayMs > maxDelayMs) delayMs = maxDelayMs;
        if (delayMs < 0.0f) delayMs = 0.0f;
        setDelaySamples(delayMs * sampleRateHz / 1000.0);
    }

    void AdjustableDelayLine::setDelaySamples(double samples)
    {
        const double longest = double(buffer.size()) - 1.0;
        if (!(samples >= 1.0)) samples = 1.0;
        if (samples > longest) samples = longest;
        delaySamples = samples;
    }

    float AdjustableDelayLine::push(float sample)
    {
        if (buffer.size() < 2) return sample;

        const int capacity = int(buffer.size());

        double readIndex = writeIndex - delaySamples;
        if (readIndex < 0.0) readIndex += capacity;
        int ri = int(readIndex);
        float f = float(readIndex - ri);
        if (ri >= capacity) ri -= capacity;
        int rj = ri + 1; if (rj >= capacity) rj -= capacity;

        float si = buffer[ri];
        float sj = buffer[rj];
        float outSample = (1.0f - f) * si + f * sj;

        buffer[writeIndex++] = flushTiny(sample + fbFraction * outSample);
        if (writeIndex >= capacity) writeIndex = 0;

        return (output = outSample);
    }

}
