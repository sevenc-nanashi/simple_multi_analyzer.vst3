#include "Analyzer.h"
#include <cmath>
#include <numeric>

Analyzer::Analyzer() : Thread("Spectrum analysis")
{
    static_assert(std::atomic<bool>::is_always_lock_free);
    for (int i = 0; i < pairCount; ++i)
        visible[i].store(i < 16);
    juce::dsp::WindowingFunction<float>::fillWindowingTables(
        window.data(), fftSize, juce::dsp::WindowingFunction<float>::hann, false);
    amplitudeScale = 2.0f / std::accumulate(window.begin(), window.end(), 0.0f);
    for (auto& spectrum : spectra)
        spectrum.fill(-100.0f);
    checkCalibration();
}

Analyzer::~Analyzer()
{
    release();
}

void Analyzer::prepare(double sampleRate)
{
    release();
    rate.store(sampleRate);
    smoothing = static_cast<float>(std::exp(-hopSize / (sampleRate * 0.150)));
    for (auto& pair : pairs)
    {
        pair.fifo.reset();
        pair.pendingSamples = 0;
        pair.nextSequence = 0;
        pair.receiving.store(false);
        pair.hasPrevious = false;
        pair.power.fill(0.0f);
    }
    {
        const juce::ScopedLock lock(spectrumLock);
        for (auto& spectrum : spectra)
            spectrum.fill(-100.0f);
    }
    startThread();
}

void Analyzer::release()
{
    stopThread(-1);
}

double Analyzer::getSampleRate() const
{
    return rate.load();
}

void Analyzer::skip(int index)
{
    auto& pair = pairs[index];
    pair.receiving.store(false);
    pair.pendingSamples = 0;
    ++pair.nextSequence;
}

void Analyzer::push(int index, const float* left, const float* right, int samples)
{
    auto& pair = pairs[index];
    pair.receiving.store(true);
    while (samples > 0)
    {
        const int count = juce::jmin(samples, hopSize - pair.pendingSamples);
        std::copy_n(left, count, pair.pending.left.data() + pair.pendingSamples);
        std::copy_n(right, count, pair.pending.right.data() + pair.pendingSamples);
        pair.pendingSamples += count;
        left += count;
        right += count;
        samples -= count;
        if (pair.pendingSamples == hopSize)
        {
            pair.pending.sequence = pair.nextSequence++;
            auto write = pair.fifo.write(1);
            if (write.blockSize1 > 0)
                pair.queue[write.startIndex1] = pair.pending;
            pair.pendingSamples = 0;
        }
    }
}

void Analyzer::transform(const float* first, const float* second, std::array<float, fftSize * 2>& work)
{
    work.fill(0.0f);
    for (int i = 0; i < hopSize; ++i)
    {
        work[i] = first[i] * window[i];
        work[i + hopSize] = second[i] * window[i + hopSize];
    }
    fft.performFrequencyOnlyForwardTransform(work.data(), true);
}

void Analyzer::analyze(Pair& pair, const Hop& next)
{
    if (pair.hasPrevious && next.sequence == pair.previous.sequence + 1)
    {
        transform(pair.previous.left.data(), next.left.data(), leftWork);
        transform(pair.previous.right.data(), next.right.data(), rightWork);
        for (int bin = 0; bin < binCount; ++bin)
        {
            const float scale = amplitudeScale * ((bin == 0 || bin == binCount - 1) ? 0.5f : 1.0f);
            const float l = leftWork[bin] * scale;
            const float r = rightWork[bin] * scale;
            const float power = (l * l + r * r) * 0.5f;
            pair.power[bin] =
                std::isfinite(power) ? smoothing * pair.power[bin] + (1.0f - smoothing) * power : 0.0f;
        }
    }
    // Keep the last valid power across dropped hops; never FFT across the gap.
    pair.previous = next;
    pair.hasPrevious = true;
}

void Analyzer::run()
{
    while (!threadShouldExit())
    {
        for (int i = 0; i < pairCount; ++i)
        {
            auto& pair = pairs[i];
            const bool active = editorOpen.load() && visible[i].load() && pair.receiving.load();
            const bool clear = !active && pair.hasPrevious;
            const int ready = pair.fifo.getNumReady();
            {
                auto read = pair.fifo.read(ready);
                read.forEach(
                    [&](int index)
                    {
                        if (active)
                            analyze(pair, pair.queue[index]);
                    });
            }
            if (clear)
            {
                pair.hasPrevious = false;
                pair.power.fill(0.0f);
            }
            if (ready > 0 || clear)
            {
                const juce::ScopedLock lock(spectrumLock);
                for (int bin = 0; bin < binCount; ++bin)
                    spectra[i][bin] = juce::Decibels::gainToDecibels(std::sqrt(pair.power[bin]), -100.0f);
            }
        }
        wait(5);
    }
}

void Analyzer::copySpectra(Spectra& destination)
{
    const juce::ScopedLock lock(spectrumLock);
    destination = spectra;
}

void Analyzer::checkCalibration()
{
#if JUCE_DEBUG
    std::array<float, fftSize> sine{};
    constexpr int bin = 64;
    for (int i = 0; i < fftSize; ++i)
        sine[i] = std::sin(juce::MathConstants<float>::twoPi * bin * i / fftSize);
    auto& pair = pairs[0];
    for (const float rightGain : {1.0f, -1.0f, 0.0f})
    {
        for (int i = 0; i < hopSize; ++i)
        {
            pair.previous.left[i] = sine[i];
            pair.previous.right[i] = sine[i] * rightGain;
            pair.pending.left[i] = sine[i + hopSize];
            pair.pending.right[i] = sine[i + hopSize] * rightGain;
        }
        pair.previous.sequence = 0;
        pair.pending.sequence = 1;
        pair.hasPrevious = true;
        analyze(pair, pair.pending);
        jassert(std::abs(pair.power[bin] - (1.0f + rightGain * rightGain) * 0.5f) < 0.001f);
    }
    pair.pending.sequence += 2;
    const auto previousPower = pair.power;
    analyze(pair, pair.pending);
    jassert(pair.power == previousPower);
    pair.hasPrevious = false;
    pair.power.fill(0.0f);
#endif
}
