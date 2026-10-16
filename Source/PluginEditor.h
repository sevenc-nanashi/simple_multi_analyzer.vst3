#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

  private:
    struct PairRow final : juce::Component
    {
        PairRow(PluginEditor&, int);
        void paint(juce::Graphics&) override;
        void resized() override;
        PluginEditor& editor;
        int index;
        juce::ToggleButton visible;
        juce::TextButton number;
        juce::Label name;
    };

    void timerCallback() override;
    juce::Rectangle<float> graphBounds() const;
    void drawSpectrum(juce::Graphics&, int, juce::Rectangle<float>, float maxFrequency);
    static juce::Colour pairColour(int index);

    PluginProcessor& processor;
    juce::LookAndFeel_V4 lookAndFeel;
    juce::Viewport viewport;
    juce::Component list;
    std::array<std::unique_ptr<PairRow>, Analyzer::pairCount> rows;
    Analyzer::Spectra spectra{};
    int selectedPair = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
