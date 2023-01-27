/*
  ==============================================================================

    Exciter.h
    Created: 26 Jan 2023 3:17:15pm
    Author:  George O'Hara

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
using juce::dsp::DelayLine;
using juce::Random;

class Exciter {
    DelayLine<float> delay;
    Random rng;
public:
    Exciter() {}
    
    void prepare(float maxLoopLen, juce::dsp::ProcessSpec spec) {
        delay.setMaximumDelayInSamples(maxLoopLen);
        delay.prepare(spec);
    }
    
    // Populate the delay line with plain white noise - this forms the impulse of our note
    void populateImpulse(DelayLine<float>& previousSampleBuffer) {
        int loopSize = previousSampleBuffer.getDelay();
        while (loopSize--) {
            float sample = (rng.nextFloat() - 0.5) * 2;
            previousSampleBuffer.pushSample(0, sample);
        }
    }
    
    // Populate the delay line with white noise pushed through a comb filter
    void impulsePicked(DelayLine<float>& previousSampleBuffer, float pickPosition) {
        jassert( 0.0 <= pickPosition && pickPosition <= 1.0);
        int loopSize = previousSampleBuffer.getDelay();
        delay.reset();
        delay.setDelay(pickPosition * loopSize);
        while (loopSize--) {
            
            float sample = (rng.nextFloat() - 0.5) * 2;
            delay.pushSample(0, sample);
            sample = sample - delay.popSample(0);
            previousSampleBuffer.pushSample(0, sample);
        }
        
    }
};
