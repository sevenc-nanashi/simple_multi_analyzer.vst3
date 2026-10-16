#pragma once

#include <array>
#include <atomic>
#include <juce_dsp/juce_dsp.h>

class Analyzer final : private juce::Thread
{
  public:
    static constexpr int pairCount = 64;
    static constexpr int fftSize = 4096;
    static constexpr int hopSize = fftSize / 2;
    static constexpr int binCount = fftSize / 2 + 1;
    using Spectrum = std::array<float, binCount>;
    using Spectra = std::array<Spectrum, pairCount>;

    Analyzer();
    ~Analyzer() override;
    void prepare(double sampleRate);
    void release();
    void push(int pair, const float* left, const float* right, int samples);
    void skip(int pair);
    void copySpectra(Spectra& destination);
    double getSampleRate() const;

    std::array<std::atomic<bool>, pairCount> visible;
    std::atomic<bool> editorOpen{false};

  private:
    struct Hop
    {
        std::array<float, hopSize> left{}, right{};
        uint64_t sequence = 0;
    };
    struct Pair
    {
        static constexpr int queueSize = 9;
        juce::AbstractFifo fifo{queueSize};
        std::array<Hop, queueSize> queue;
        Hop pending, previous;
        int pendingSamples = 0;
        uint64_t nextSequence = 0;
        std::atomic<bool> receiving{false};
        bool hasPrevious = false;
        Spectrum power{};
    };

    void run() override;
    void analyze(Pair& pair, const Hop& next);
    void transform(const float* first, const float* second, std::array<float, fftSize * 2>& work);
    void checkCalibration();

    std::array<Pair, pairCount> pairs;
    juce::dsp::FFT fft{12};
    std::array<float, fftSize> window{};
    std::array<float, fftSize * 2> leftWork{}, rightWork{};
    float amplitudeScale = 0.0f;
    float smoothing = 0.0f;
    std::atomic<double> rate{48000.0};
    juce::CriticalSection spectrumLock;
    Spectra spectra{};
};
