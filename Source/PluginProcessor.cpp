/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <vector>

struct SineWaveSound: public juce::SynthesiserSound {
    SineWaveSound() {}
    
    bool appliesToNote(int) override {return true;}
    bool appliesToChannel(int) override {return true;}
};

struct SineWaveVoice: public juce::SynthesiserVoice {
    double currentAngle, angleDelta, level = 0.0;
    SineWaveVoice(){}
    
    bool canPlaySound (juce::SynthesiserSound* sound) override {
        return true;
    }
    
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        currentAngle = 0.0;
        level = velocity * 0.15;
        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();
        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
    }
    
    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override {
        if (angleDelta != 0.0) {
            for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++) {
                for(int i = 0; i < numSamples; i++) {
                    auto currentSample = (float) (std::sin(currentAngle) * level);
                    outputBuffer.addSample(channel, startSample + i, currentSample);
                    currentAngle += angleDelta;
                }
            }
        }
    }
    
    void stopNote(float, bool) override {
        clearCurrentNote();
        angleDelta = 0.0;
    }
    
    virtual void controllerMoved(int,int) override {}
    virtual void pitchWheelMoved(int) override {}
};

struct KsSound: public juce::SynthesiserSound {
    KsSound() {}
    
    bool appliesToNote(int) override {return true;}
    bool appliesToChannel(int) override {return true;}
};

struct KsVoice: public juce::SynthesiserVoice {
    double level = 0.0;
    // TODO: change to use a proper circular buffer
    std::queue<float> previous_samples;
    juce::Random random;
    bool isPlaying;
    float prev_for_lowpass = 0.0;
    
    KsVoice(){}
    
    bool canPlaySound (juce::SynthesiserSound* sound) override {
        return true;
    }
    
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        for(int i = 0; i < 50; i++) {
            previous_samples.push(random.nextFloat() - 0.5);
        }
        
        level = velocity * 0.15;
        isPlaying = true;
    }
    
    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override {
        if (isPlaying) {


            for (int channel = 0; channel < outputBuffer.getNumChannels(); channel++) {
                for(int i = 0; i < numSamples; i++) {

                    // Get previous sample
                    auto input_sample = previous_samples.front();
                    previous_samples.pop();
                    // Lowpass it
                    float output_sample = 0.5 * (prev_for_lowpass + input_sample);
                    prev_for_lowpass = input_sample;
                    outputBuffer.addSample(channel, startSample + i, output_sample);
                    previous_samples.push(output_sample);
                }
            }
        }
    }
    
    void stopNote(float, bool) override {
        clearCurrentNote();
        isPlaying = false;
        prev_for_lowpass = 0.0;
        while(!previous_samples.empty()) {
            previous_samples.pop();
        }
    }
    
    virtual void controllerMoved(int,int) override {}
    virtual void pitchWheelMoved(int) override {}
};



//==============================================================================
KarplusStrongAudioProcessor::KarplusStrongAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
//                       .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::mono(), true)
                     #endif
                       )
#endif
{
    for (auto i = 0; i < 1; i++) {
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
