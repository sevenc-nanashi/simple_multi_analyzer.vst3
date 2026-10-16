#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessor::BusesProperties PluginProcessor::makeBuses()
{
    BusesProperties buses;
    for (int i = 0; i < Analyzer::pairCount; ++i)
    {
        const auto number = juce::String(i + 1).paddedLeft('0', 2);
        buses.addBus(true, "Input " + number + " L/R", juce::AudioChannelSet::stereo(), true);
        buses.addBus(false, "Output " + number + " L/R", juce::AudioChannelSet::stereo(), true);
    }
    return buses;
}

PluginProcessor::PluginProcessor() : AudioProcessor(makeBuses()), analyzer(std::make_unique<Analyzer>())
{
    for (int i = 0; i < Analyzer::pairCount; ++i)
        names[i] = "Pair " + juce::String(i + 1);
}

const juce::String PluginProcessor::getName() const
{
    return "simple_multi_analyzer.vst3";
}

void PluginProcessor::prepareToPlay(double sampleRate, int)
{
    analyzer->prepare(sampleRate);
}

void PluginProcessor::releaseResources()
{
    analyzer->release();
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    if (layout.inputBuses.size() != Analyzer::pairCount || layout.outputBuses.size() != Analyzer::pairCount)
        return false;
    for (int i = 0; i < Analyzer::pairCount; ++i)
    {
        const auto& input = layout.inputBuses[i];
        if (input != layout.outputBuses[i] ||
            (input != juce::AudioChannelSet::stereo() && !input.isDisabled()))
            return false;
    }
    return !layout.inputBuses[0].isDisabled();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < Analyzer::pairCount; ++i)
    {
        if (!analyzer->editorOpen.load() || !analyzer->visible[i].load() || !getBus(true, i)->isEnabled())
        {
            analyzer->skip(i);
            continue;
        }
        auto input = getBusBuffer(buffer, true, i);
        analyzer->push(i, input.getReadPointer(0), input.getReadPointer(1), buffer.getNumSamples());
    }
    // Matching input/output layouts let the host's in-place buffer pass through untouched.
}

bool PluginProcessor::hasEditor() const
{
    return true;
}
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}
bool PluginProcessor::acceptsMidi() const
{
    return false;
}
bool PluginProcessor::producesMidi() const
{
    return false;
}
double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}
int PluginProcessor::getNumPrograms()
{
    return 1;
}
int PluginProcessor::getCurrentProgram()
{
    return 0;
}
void PluginProcessor::setCurrentProgram(int) {}
const juce::String PluginProcessor::getProgramName(int)
{
    return {};
}
void PluginProcessor::changeProgramName(int, const juce::String&) {}

juce::String PluginProcessor::getPairName(int index) const
{
    const juce::ScopedLock lock(stateLock);
    return names[index];
}
void PluginProcessor::setPairName(int index, const juce::String& name)
{
    {
        const juce::ScopedLock lock(stateLock);
        names[index] = name.substring(0, 64);
    }
    updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
}
void PluginProcessor::setPairVisible(int index, bool visible)
{
    analyzer->visible[index].store(visible);
    updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
}
juce::Point<int> PluginProcessor::getEditorSize() const
{
    const juce::ScopedLock lock(stateLock);
    return editorSize;
}
void PluginProcessor::setEditorSize(int width, int height)
{
    {
        const juce::ScopedLock lock(stateLock);
        if (editorSize == juce::Point<int>(width, height))
            return;
        editorSize = {width, height};
    }
    updateHostDisplay(ChangeDetails().withNonParameterStateChanged(true));
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    const juce::ScopedLock lock(stateLock);
    juce::XmlElement xml("SimpleMultiAnalyzer");
    xml.setAttribute("version", 1);
    xml.setAttribute("width", editorSize.x);
    xml.setAttribute("height", editorSize.y);
    for (int i = 0; i < Analyzer::pairCount; ++i)
    {
        auto* pair = xml.createNewChildElement("Pair");
        pair->setAttribute("name", names[i]);
        pair->setAttribute("visible", analyzer->visible[i].load());
    }
    copyXmlToBinary(xml, destination);
}

void PluginProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName("SimpleMultiAnalyzer") || xml->getIntAttribute("version") != 1 ||
        xml->getNumChildElements() != Analyzer::pairCount)
        return;
    const int width = xml->getIntAttribute("width");
    const int height = xml->getIntAttribute("height");
    if (width < 800 || width > 2000 || height < 480 || height > 1400)
        return;
    std::array<juce::String, Analyzer::pairCount> newNames;
    std::array<bool, Analyzer::pairCount> newVisible{};
    for (int i = 0; i < Analyzer::pairCount; ++i)
    {
        auto* pair = xml->getChildElement(i);
        const auto visibility = pair->getStringAttribute("visible");
        if (!pair->hasTagName("Pair") || !pair->hasAttribute("name") ||
            (visibility != "0" && visibility != "1"))
            return;
        newNames[i] = pair->getStringAttribute("name").substring(0, 64);
        newVisible[i] = visibility == "1";
    }
    const juce::ScopedLock lock(stateLock);
    names = std::move(newNames);
    editorSize = {width, height};
    for (int i = 0; i < Analyzer::pairCount; ++i)
        analyzer->visible[i].store(newVisible[i]);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
