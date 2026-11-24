#pragma once
#include "PluginProcessor.h"

class CoilEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CoilEditor (CoilProcessor&);
    ~CoilEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    CoilProcessor& proc;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    std::array<Knob, 15> knobs;
    std::array<juce::TextButton, 4> modelButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CoilEditor)
};
