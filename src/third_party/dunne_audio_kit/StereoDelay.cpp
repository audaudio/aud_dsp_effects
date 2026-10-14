// Copyright AudioKit. All Rights Reserved.
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm): see
// StereoDelay.h - renderFrame() renders one frame and render() runs on it,
// setDelaySamples() sets the delay in frames, and the mix is 0 dry, 1 wet
// (the original's mix was inverted). init() keeps the sample rate and the
// feedback set before it, which the lines' init() reset. <cmath> is
// included for std::fmax.

#include "StereoDelay.h"

#include <cmath>

namespace DunneCore
{
    void StereoDelay::init(double sampleRate, double maxDelayMs)
    {
        sampleRateHz = sampleRate;
        delayLine1.init(sampleRate, maxDelayMs);
        delayLine2.init(sampleRate, maxDelayMs);
        setFeedback(feedbackFraction);
    }

    void StereoDelay::deinit()
    {
        delayLine1.deinit();
        delayLine2.deinit();
    }

    void StereoDelay::clear()
    {
        delayLine1.clear();
        delayLine2.clear();
    }

    void StereoDelay::setPingPongMode(bool pingPong)
    {
        pingPongMode = pingPong;
        setFeedback(feedbackFraction);
    }

    void StereoDelay::setDelayMs(double delayMs)
    {
        delayLine1.setDelayMs(std::fmax(1.0, delayMs));
        delayLine2.setDelayMs(std::fmax(1.0, delayMs));
    }

    void StereoDelay::setDelaySamples(double samples)
    {
        delayLine1.setDelaySamples(samples);
        delayLine2.setDelaySamples(samples);
    }

    void StereoDelay::setFeedback(float fraction)
    {
        feedbackFraction = fraction;
        delayLine1.setFeedback(pingPongMode ? 0.0f : fraction);
        delayLine2.setFeedback(pingPongMode ? 0.0f : fraction);
    }

    void StereoDelay::setDryWetMix(float fraction)
    {
        dryWetMixFraction = fraction;
    }

    void StereoDelay::renderFrame(float inLeft, float inRight, float &outLeft, float &outRight)
    {
        float leftSample, rightSample;
        if (pingPongMode)
        {
            float inputSample = 0.5f * (inLeft + inRight);
            leftSample = delayLine1.push(inputSample + feedbackFraction * delayLine2.getOutput());
            rightSample = delayLine2.push(leftSample);
        }
        else
        {
            leftSample = delayLine1.push(inLeft);
            rightSample = delayLine2.push(inRight);
        }

        outLeft = (1.0f - dryWetMixFraction) * inLeft + dryWetMixFraction * leftSample;
        outRight = (1.0f - dryWetMixFraction) * inRight + dryWetMixFraction * rightSample;
    }

    void StereoDelay::render(int sampleCount, const float *inBuffers[], float *outBuffers[])
    {
        for (int i = 0; i < sampleCount; i++)
        {
            renderFrame(inBuffers[0][i], inBuffers[1][i], outBuffers[0][i], outBuffers[1][i]);
        }
    }
}
