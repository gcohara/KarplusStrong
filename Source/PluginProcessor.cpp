/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Filters.h"
#include "Utils.h"
#include "Exciter.h"
#include <vector>

using juce::dsp::DelayLine;
using juce::MidiMessage;


struct KsSound: public juce::SynthesiserSound {
    KsSound() {}
    bool appliesToNote(int) override {return true;}
    bool appliesToChannel(int) override {return true;}
};

class KsVoice: public juce::SynthesiserVoice {
    // Parameters that should be user adjustable
public:
    float pickPosition = 0.5;
    
    // Not parameters
private:
    DelayLine<float> previousSamples;
    Exciter exciter;
    LowPass lp;
    AllPass ap;
    bool isPlaying = false;
    float level = 0.0;
    
public:
    KsVoice(){
        // Set up the delay line
        juce::dsp::ProcessSpec spec { getSampleRate(), 1024, 1 };
        previousSamples.prepare(spec);
        int maxLoopLen = getSampleRate() / MidiMessage::getMidiNoteInHertz(0);
        previousSamples.setMaximumDelayInSamples(maxLoopLen);
        exciter.prepare(maxLoopLen, spec);
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override {
        return true;
    }
    
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        float fundamentalFreq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        auto [requiredPreviousSamples, requiredPhaseDelay] = calculateRequiredDelays(fundamentalFreq);
        ap.updateCoefficient(requiredPhaseDelay, fundamentalFreq, getSampleRate());
        previousSamples.setDelay(requiredPreviousSamples);
//        exciter.populateImpulse(previousSamples);
        exciter.impulsePicked(previousSamples, pickPosition);
        level = velocity;
        isPlaying = true;
    }
    
    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override {
        if (isPlaying) {
            for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++) {
                for(int i = 0; i < numSamples; i++) {
                    // Get previous sample
                    float inputSample = previousSamples.popSample(0);
                    // Lowpass it
                    float intermediateSample = lp.getNextSample(inputSample);
                    // Allpass it
                    float outputSample = ap.getNextOutput(intermediateSample);
                    // Push it to delay line
                    previousSamples.pushSample(0, outputSample);
                    outputBuffer.addSample(channel, startSample + i, outputSample * level);
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
    
private:
    std::tuple<float, float> calculateRequiredDelays(float fundamentalFreq) {
        float requiredLoopDelay = getSampleRate() / fundamentalFreq;
        float requiredPreviousSamples = floor(requiredLoopDelay - 0.5);
        auto requiredPhaseDelay = requiredLoopDelay - 0.5 - requiredPreviousSamples;
        return {requiredPreviousSamples, requiredPhaseDelay};
    }
};



//==============================================================================
KarplusStrongAudioProcessor::KarplusStrongAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)
                       ){
    for (auto i = 0; i < 6; i++) {
        synth.addVoice(new KsVoice());
    };
    synth.addSound(new KsSound());
         addParameter(pickPosition = new juce::AudioParameterFloat(juce::ParameterID { "pickPosition",  1 }, "Pick Position", 0.0f, 1.0f, 0.5f));

}

KarplusStrongAudioProcessor::~KarplusStrongAudioProcessor()
{
}

//==============================================================================

void KarplusStrongAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void KarplusStrongAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data in case they contain nonzero data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    for (int i = 0; i < synth.getNumVoices(); i++) {
        auto voice = dynamic_cast<KsVoice*>(synth.getVoice(i));
        voice->pickPosition = pickPosition->get();
    }
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
//=======================HERE BE BOILERPLATE====================================
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
