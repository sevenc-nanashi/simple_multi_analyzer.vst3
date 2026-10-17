#include "PluginEditor.h"
#include <cmath>

namespace
{
const juce::Colour background(0xff10151d);
const juce::Colour panel(0xff18212c);
const juce::Colour text(0xffe5edf5);
const juce::Colour muted(0xff94a6b9);
constexpr int rowHeight = 34;
constexpr int sidebarWidth = 244;
} // namespace

juce::Colour PluginEditor::pairColour(int index)
{
    return juce::Colour::fromHSV(std::fmod(0.53f + static_cast<float>(index) * 0.618034f, 1.0f), 0.65f, 0.96f,
                                 1.0f);
}

PluginEditor::PairRow::PairRow(PluginEditor& owner, int pairIndex) : editor(owner), index(pairIndex)
{
    visible.setToggleState(editor.processor.analyzer->visible[index].load(), juce::dontSendNotification);
    visible.setTitle("Show pair " + juce::String(index + 1));
    visible.setTooltip("Show / hide spectrum (audio is unchanged)");
    visible.onClick = [this] { editor.processor.setPairVisible(index, visible.getToggleState()); };
    addAndMakeVisible(visible);

    number.setButtonText(juce::String(index + 1).paddedLeft('0', 2));
    number.setColour(juce::TextButton::textColourOffId, pairColour(index));
    number.setTooltip("Highlight this pair");
    number.onClick = [this]
    {
        editor.selectedPair = index;
        editor.repaint();
        editor.list.repaint();
    };
    addAndMakeVisible(number);

    name.setText(editor.processor.getPairName(index), juce::dontSendNotification);
    name.setEditable(false, true);
    name.setTitle("Name of pair " + juce::String(index + 1));
    name.setTooltip("Double-click to rename");
    name.onTextChange = [this] { editor.processor.setPairName(index, name.getText()); };
    name.onEditorShow = [this] { name.getCurrentTextEditor()->setInputRestrictions(64); };
    addAndMakeVisible(name);
}

void PluginEditor::PairRow::paint(juce::Graphics& g)
{
    if (editor.selectedPair == index)
    {
        g.setColour(pairColour(index).withAlpha(0.13f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f);
    }
}

void PluginEditor::PairRow::resized()
{
    auto bounds = getLocalBounds().reduced(4, 3);
    visible.setBounds(bounds.removeFromLeft(28));
    number.setBounds(bounds.removeFromLeft(34).reduced(1));
    name.setBounds(bounds.reduced(4, 0));
}

PluginEditor::PluginEditor(PluginProcessor& owner) : AudioProcessorEditor(owner), processor(owner)
{
    setName("simple_multi_analyzer.vst3");
    lookAndFeel.setColour(juce::Label::textColourId, text);
    lookAndFeel.setColour(juce::TextButton::buttonColourId, panel);
    lookAndFeel.setColour(juce::ToggleButton::tickColourId, text);
    lookAndFeel.setColour(juce::TextEditor::backgroundColourId, background);
    lookAndFeel.setColour(juce::TextEditor::textColourId, text);
    setLookAndFeel(&lookAndFeel);
    for (int i = 0; i < Analyzer::pairCount; ++i)
    {
        rows[i] = std::make_unique<PairRow>(*this, i);
        list.addAndMakeVisible(*rows[i]);
        spectra[i].fill(-100.0f);
    }
    viewport.setViewedComponent(&list, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    const auto size = processor.getEditorSize();
    setResizable(true, true);
    setResizeLimits(800, 480, 2000, 1400);
    setSize(size.x, size.y);
    processor.analyzer->editorOpen.store(true);
    startTimerHz(30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    processor.analyzer->editorOpen.store(false);
    viewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}

juce::Rectangle<float> PluginEditor::graphBounds() const
{
    return {64.0f, 100.0f, static_cast<float>(getWidth() - sidebarWidth - 100),
            static_cast<float>(getHeight() - 170)};
}

void PluginEditor::resized()
{
    viewport.setBounds(getWidth() - sidebarWidth, 100, sidebarWidth - 12, getHeight() - 124);
    list.setSize(viewport.getWidth() - viewport.getScrollBarThickness(), rowHeight * Analyzer::pairCount);
    for (int i = 0; i < Analyzer::pairCount; ++i)
        rows[i]->setBounds(0, i * rowHeight, list.getWidth(), rowHeight);
    processor.setEditorSize(getWidth(), getHeight());
}

void PluginEditor::timerCallback()
{
    processor.analyzer->copySpectra(spectra);
    const auto size = processor.getEditorSize();
    if (size.x != getWidth() || size.y != getHeight())
        setSize(size.x, size.y);
    for (int i = 0; i < Analyzer::pairCount; ++i)
    {
        rows[i]->visible.setToggleState(processor.analyzer->visible[i].load(), juce::dontSendNotification);
        if (!rows[i]->name.isBeingEdited())
            rows[i]->name.setText(processor.getPairName(i), juce::dontSendNotification);
    }
    repaint();
}

void PluginEditor::drawSpectrum(juce::Graphics& g, int index, juce::Rectangle<float> graph, float maximum)
{
    if (!processor.analyzer->visible[index].load())
        return;
    const float binWidth = static_cast<float>(processor.analyzer->getSampleRate()) / Analyzer::fftSize;
    const float logRange = std::log(maximum / 20.0f);
    std::array<double, Analyzer::binCount + 1> integratedPower{};
    for (int bin = 0; bin < Analyzer::binCount; ++bin)
        integratedPower[bin + 1] = integratedPower[bin] + std::pow(10.0, spectra[index][bin] * 0.1);
    const auto integral = [&](double position)
    {
        const int bin = juce::jmin(static_cast<int>(position), Analyzer::binCount - 1);
        return juce::jmap(position - bin, integratedPower[bin], integratedPower[bin + 1]);
    };
    const double bandRatio = std::exp2(1.0 / 24.0);
    juce::Path path;
    const int pixels = static_cast<int>(graph.getWidth());
    for (int x = 0; x <= pixels; ++x)
    {
        const float frequency = 20.0f * std::exp(logRange * static_cast<float>(x) / pixels);
        const float position = frequency / binWidth;
        const int first = juce::jlimit(0, Analyzer::binCount - 2, static_cast<int>(position));
        const float fraction = juce::jlimit(0.0f, 1.0f, position - first);
        float db = juce::jmap(fraction, spectra[index][first], spectra[index][first + 1]);
        const double lower =
            juce::jlimit(0.0, static_cast<double>(Analyzer::binCount), position / bandRatio + 0.5);
        const double upper =
            juce::jlimit(0.0, static_cast<double>(Analyzer::binCount), position * bandRatio + 0.5);
        const double width = upper - lower;
        if (width > 1.0)
        {
            // Fractional bin weights avoid steps as the 1/12-octave band moves across bins.
            const double power = (integral(upper) - integral(lower)) / width;
            const float smoothedDb = static_cast<float>(10.0 * std::log10(juce::jmax(1.0e-10, power)));
            db = juce::jmap(static_cast<float>(juce::jmin(1.0, width - 1.0)), db, smoothedDb);
        }
        jassert(std::isfinite(db) && db >= -100.01f);
        const float y =
            juce::jmap(juce::jlimit(-100.0f, 6.0f, db), -100.0f, 6.0f, graph.getBottom(), graph.getY());
        if (x == 0)
            path.startNewSubPath(graph.getX(), y);
        else
            path.lineTo(graph.getX() + x, y);
    }
    const bool selected = index == selectedPair;
    g.setColour(pairColour(index).withAlpha(selected ? 1.0f : 0.65f));
    g.strokePath(path, juce::PathStrokeType(selected ? 2.2f : 1.2f));
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(text);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("simple_multi_analyzer.vst3", 24, 18, getWidth() - 48, 30, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Simple Multi Analyzer / https://github.com/sevenc-nanashi/simple_multi_analyzer.vst3", 25, 53,
               getWidth() - 50, 20, juce::Justification::centredLeft);

    const auto graph = graphBounds();
    g.setColour(panel);
    g.fillRoundedRectangle(graph.expanded(1.0f), 6.0f);
    const float maximum = juce::jmin(20000.0f, static_cast<float>(processor.analyzer->getSampleRate() * 0.5));
    g.setFont(juce::FontOptions(12.0f));
    for (int db = -100; db <= 0; db += 20)
    {
        const float y = juce::jmap(static_cast<float>(db), -100.0f, 6.0f, graph.getBottom(), graph.getY());
        g.setColour(muted.withAlpha(0.14f));
        g.drawHorizontalLine(static_cast<int>(y), graph.getX(), graph.getRight());
        g.setColour(muted);
        g.drawText(juce::String(db), 12, static_cast<int>(y) - 8, 42, 16, juce::Justification::centredRight);
    }
    for (const float hz :
         {20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f})
    {
        if (hz > maximum)
            continue;
        const float x = graph.getX() + graph.getWidth() * std::log(hz / 20.0f) / std::log(maximum / 20.0f);
        g.setColour(muted.withAlpha(0.14f));
        g.drawVerticalLine(static_cast<int>(x), graph.getY(), graph.getBottom());
        g.setColour(muted);
        const auto label = hz >= 1000.0f ? juce::String(hz / 1000.0f, 0) + "k" : juce::String(hz, 0);
        g.drawText(label, static_cast<int>(x) - 22, static_cast<int>(graph.getBottom()) + 8, 44, 18,
                   juce::Justification::centred);
    }
    g.setColour(muted);
    g.drawText("dBFS", 14, 78, 44, 18, juce::Justification::centredRight);
    g.drawText("Hz", static_cast<int>(graph.getRight()) - 20, getHeight() - 36, 30, 18,
               juce::Justification::centredRight);
    g.drawText("PAIRS  /  DOUBLE-CLICK TO RENAME", getWidth() - sidebarWidth + 4, 78, sidebarWidth - 12, 18,
               juce::Justification::centredLeft);

    {
        const juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(graph.toNearestInt());
        for (int i = 0; i < Analyzer::pairCount; ++i)
            if (i != selectedPair)
                drawSpectrum(g, i, graph, maximum);
        drawSpectrum(g, selectedPair, graph, maximum);
    }
    g.setColour(pairColour(selectedPair));
    g.drawText(juce::String(selectedPair + 1).paddedLeft('0', 2) + "  " + processor.getPairName(selectedPair),
               64, getHeight() - 39, getWidth() - sidebarWidth - 120, 22, juce::Justification::centredLeft);
}
