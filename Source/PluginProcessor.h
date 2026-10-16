#pragma once

#include "Analyzer.h"
#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessor final : public juce::AudioProcessor
{
  public:
    PluginProcessor();
    const juce::String getName() const override;
    void prepareToPlay(double sampleRate, int maximumBlockSize) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::String getPairName(int index) const;
    void setPairName(int index, const juce::String& name);
    void setPairVisible(int index, bool visible);
    juce::Point<int> getEditorSize() const;
    void setEditorSize(int width, int height);
    std::unique_ptr<Analyzer> analyzer;

  private:
    static BusesProperties makeBuses();
    mutable juce::CriticalSection stateLock;
    std::array<juce::String, Analyzer::pairCount> names;
    juce::Point<int> editorSize{1100, 680};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
