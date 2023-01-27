/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KarplusStrongAudioProcessorEditor::KarplusStrongAudioProcessorEditor (KarplusStrongAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
    pickPosition.setRange(0.0, 1.0);
    addAndMakeVisible(&pickPosition);
    pickPosition.addListener(this);
    pickPosition.setSliderStyle (juce::Slider::LinearBarVertical);
    pickPosition.setRange (0.0, 1.0, 0.05);
    pickPosition.setTextBoxStyle (juce::Slider::NoTextBox, false, 90, 0);
    pickPosition.setPopupDisplayEnabled (true, false, this);
    pickPosition.setTextValueSuffix (" Posn");
    pickPosition.setValue(0.5);
}

KarplusStrongAudioProcessorEditor::~KarplusStrongAudioProcessorEditor()
{
}

//==============================================================================
void KarplusStrongAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void KarplusStrongAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    pickPosition.setBounds (40, 30, 20, getHeight() - 60);
}

void KarplusStrongAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    *audioProcessor.pickPosition = static_cast<float>(pickPosition.getValue()); 
}
