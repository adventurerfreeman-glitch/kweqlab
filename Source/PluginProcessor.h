#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KwLab.h"

kwlab::Params readParams (juce::AudioProcessorValueTreeState& apvts);

class LabProcessor : public juce::AudioProcessor
{
public:
    LabProcessor();
    ~LabProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "KW EQ Lab"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // A/B comparison: two full snapshots of every control
    void selectSlot (int slot);
    void copyCurrentToOtherSlot();
    int  getSlot() const { return slot; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double sr = 44100.0;
    kwlab::Channel chans[2];
    kwlab::Params cur;
    int lastArch = -1;
    int slot = 0;
    juce::ValueTree slotState[2];
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabProcessor)
};
