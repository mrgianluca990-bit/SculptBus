#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SculptPreciseLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class SculptBusAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SculptBusAudioProcessorEditor (SculptBusAudioProcessor&);
    ~SculptBusAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    SculptBusAudioProcessor& processor;
    SculptPreciseLookAndFeel look;
    juce::Image background;
    struct KnobUI { juce::Slider slider; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; };
    KnobUI low, mid, high, presence, output;
    juce::ToggleButton variationButton { "VAR" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> variationAttachment;
    void setupKnob (KnobUI&, const juce::String&, bool=false);
    juce::Rectangle<int> scaledRect (float x, float y, float w, float h) const;
    void drawSegmentMeter (juce::Graphics&, juce::Rectangle<float>, float, int);
    void drawAllMeters (juce::Graphics&);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SculptBusAudioProcessorEditor)
};
