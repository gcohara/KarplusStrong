//
//  Filters.h
//  KarplusStrong
//
//  Created by George O'Hara on 26/01/2023.
//

#ifndef Filters_h
#define Filters_h

class LowPass {
    float previousSample = 0.0;
    float probability = 1.0;
    juce::Random random;
    
public:
    float getNextSample(float currentSample) {
        float coeff = (random.nextFloat() < probability) ? 0.5 : -0.5;
        float outputSample = coeff * (currentSample + previousSample);
        previousSample = currentSample;
        return outputSample;
    }
    
    void clearState() {
        previousSample = 0.0;
    }
};

class AllPass {
    float a = 0.5;
    float previousInput = 0.0;
    float previousOutput = 0.0;
public:
    float getNextOutput(float currentInput) {
        float outputSample = a * (currentInput - previousOutput) + previousInput;
        previousOutput = outputSample;
        previousInput = currentInput;
        return outputSample;
    }
    
    void updateCoefficient(float phaseDelay, float fundamentalFreq, float sampleFreq) {
        auto omega_0 = juce::MathConstants<float>::twoPi * fundamentalFreq / sampleFreq;
        a = sin((1 - phaseDelay) * omega_0 / 2) / sin((1 + phaseDelay) * omega_0 / 2);
    }
    
    void clearState() {
        previousInput = 0.0;
        previousOutput = 0.0;
    }
};


#endif /* Filters_h */
