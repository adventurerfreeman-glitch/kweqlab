#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Live response display for the selected architecture (white = total, colours = each section)
class LabCurve : public juce::Component, private juce::Timer
{
public:
    explicit LabCurve (LabProcessor& p);
    void paint (juce::Graphics&) override;
    static juce::Colour groupColour (int g);
private:
    void timerCallback() override { repaint(); }
    LabProcessor& proc;
};

class LabEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LabEditor (LabProcessor&);
    ~LabEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Knob
    {
        int arch = -1;                       // -1 = shown for every architecture
        juce::Slider slider; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Combo
    {
        int arch = -1;
        juce::ComboBox box; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    void addKnob (int arch, const juce::String& id, const juce::String& text, juce::Colour c,
                  const juce::StringArray& steps = {}, const juce::String& suffix = {});
    void addCombo (int arch, const juce::String& id, const juce::String& text, const juce::StringArray& items);
    void timerCallback() override;
    void applyVisibility();

    LabProcessor& proc;
    LabCurve curve;
    juce::ComboBox archBox; juce::Label archLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> archAtt;
    juce::TextButton bypassBtn { "BYPASS" }, levelBtn { "LEVEL MATCH" }, aBtn { "A" }, bBtn { "B" }, copyBtn { "Copy to other" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, levelAtt;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> combos;
    int shownArch = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabEditor)
};
