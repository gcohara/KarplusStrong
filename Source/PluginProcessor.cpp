/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <vector>

class LowPass {
    float previousSample = 0.0;
public:
    float getNextSample(float currentSample) {
        float outputSample = 0.5 * (currentSample + previousSample);
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

class RingBuffer {
    // will fail in the niche use case of 96kHz and notes below midi G-1 (24.5Hz)
    float buffer[4096];
    juce::AbstractFifo abstractFifo { 4096 };
    juce::Random random;
public:
    void reset() {
        abstractFifo.reset();
    }
    void setSize(int newSize) {
        abstractFifo.setTotalSize(newSize);
    }
    float nextSample() {
        auto readHandle = abstractFifo.read(1);
        if (readHandle.blockSize1 == 1) {
            return buffer[readHandle.startIndex1];
        } else {
            return buffer[readHandle.startIndex2];
        }
    }
    
    void writeSample(float sample) {
        auto writeHandle = abstractFifo.write(1);
        if (writeHandle.blockSize1 == 1) {
            buffer[writeHandle.startIndex1] = sample;
        } else {
            buffer[writeHandle.startIndex2] = sample;
        }
    }
    
    void setImpulse(int loopSize) {
        setSize(loopSize);
        auto remaining_space = abstractFifo.getFreeSpace();
        while(remaining_space--) {
            writeSample(random.nextFloat() - 0.5);
        }
    }
};

struct KsSound: public juce::SynthesiserSound {
    KsSound() {}
    
    bool appliesToNote(int) override {return true;}
    bool appliesToChannel(int) override {return true;}
};

struct KsVoice: public juce::SynthesiserVoice {
    double level = 0.0;
    // TODO: change to use a proper circular buffer
//    std::queue<float> previousSamples;
    RingBuffer previousSamples;
    juce::Random random;
    bool isPlaying;
    LowPass lp;
    AllPass ap;
    
    KsVoice(){}
    
    bool canPlaySound (juce::SynthesiserSound* sound) override {
        return true;
    }
    
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        float fundamentalFreq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        float requiredLoopDelay = getSampleRate() / fundamentalFreq;
        float requiredPreviousSamples = floor(requiredLoopDelay - 0.5);
        auto requiredPhaseDelay = requiredLoopDelay - 0.5 - requiredPreviousSamples;
        ap.updateCoefficient(requiredPhaseDelay, fundamentalFreq, getSampleRate());
        previousSamples.setSize(requiredLoopDelay);
        previousSamples.setImpulse(requiredLoopDelay);
        
        level = velocity * 0.5;
        isPlaying = true;
    }
    
    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override {
        if (isPlaying) {
            for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++) {
                for(int i = 0; i < numSamples; i++) {
                    // Get previous sample
//                    float inputSample = previousSamples.front();
//                    previousSamples.pop();
                    float inputSample = previousSamples.nextSample();
                    // Lowpass it
                    float intermediateSample = lp.getNextSample(inputSample);
                    // Allpass it
                    float outputSample = ap.getNextOutput(intermediateSample);
                    previousSamples.writeSample(outputSample);
                    outputBuffer.addSample(channel, startSample + i, outputSample);
                }
            }
        }
    }
    
    void stopNote(float, bool) override {
        clearCurrentNote();
        isPlaying = false;
        lp.clearState();
        ap.clearState();
        previousSamples.reset();
    }
    
    virtual void controllerMoved(int,int) override {}
    virtual void pitchWheelMoved(int) override {}
};



//==============================================================================
KarplusStrongAudioProcessor::KarplusStrongAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)
                     #endif
                       )
#endif
{
    for (auto i = 0; i < 6; i++) {
        synth.addVoice(new KsVoice());
    };
    synth.addSound(new KsSound());
}

KarplusStrongAudioProcessor::~KarplusStrongAudioProcessor()
{
}

//==============================================================================

const juce::String KarplusStrongAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KarplusStrongAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KarplusStrongAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KarplusStrongAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KarplusStrongAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KarplusStrongAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int KarplusStrongAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KarplusStrongAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KarplusStrongAudioProcessor::getProgramName (int index)
{
    return {};
}

void KarplusStrongAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void KarplusStrongAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void KarplusStrongAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KarplusStrongAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void KarplusStrongAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool KarplusStrongAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* KarplusStrongAudioProcessor::createEditor()
{
    return new KarplusStrongAudioProcessorEditor (*this);
}

//==============================================================================
void KarplusStrongAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void KarplusStrongAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KarplusStrongAudioProcessor();
}
