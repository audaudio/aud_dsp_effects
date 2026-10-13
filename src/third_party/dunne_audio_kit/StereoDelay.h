// Copyright AudioKit. All Rights Reserved.
//
// Modified by Audanika for aud_dsp_effects (ticket 28 of aud_audio_pm):
// renderFrame() renders one frame from two input samples, so that a caller
// moves the delay, the feedback and the mix frame by frame and passes a mono
// input as the same sample twice; render() runs on it. setDelaySamples()
// sets the delay in frames. The dry/wet mix is no longer inverted: 0 is dry
// and 1 is wet, where the original rendered (1 - mix) * wet + mix * dry.

#pragma once
#include "AdjustableDelayLine.h"

namespace DunneCore
{
    class StereoDelay {
        double sampleRateHz;
        float feedbackFraction;
        float dryWetMixFraction;
        bool pingPongMode;

        AdjustableDelayLine delayLine1, delayLine2;

    public:
        StereoDelay() : sampleRateHz(0.0), feedbackFraction(0.0f), dryWetMixFraction(0.5f), pingPongMode(false) {}
        ~StereoDelay() { deinit(); }

        void init(double sampleRate, double maxDelayMs);
        void deinit();

        void clear();

        void setPingPongMode(bool pingPong);
        void setDelayMs(double delayMs);
        void setDelaySamples(double samples);
        void setFeedback(float fraction);
        void setDryWetMix(float fraction);

        bool getPingPongMode() { return pingPongMode; }

        void renderFrame(float inLeft, float inRight, float &outLeft, float &outRight);
        void render(int sampleCount, const float *inBuffers[], float *outBuffers[]);
    };

}
