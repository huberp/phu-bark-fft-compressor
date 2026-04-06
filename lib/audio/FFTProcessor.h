#pragma once

#include "AudioSampleFifo.h"
#include <juce_dsp/juce_dsp.h>

namespace phu {
namespace audio {

/**
 * FFT processor for spectrum analysis, designed to run on the UI thread.
 *
 * Reads samples from an AudioSampleFifo<2> (stereo), applies windowing,
 * computes FFT, and produces a smoothed magnitude spectrum for visualization.
 *
 * Uses a sliding window approach: a local mono ring buffer holds the last
 * fftSize samples. Each process() call drains only the NEW samples from the
 * FIFO and shifts them in, then recomputes the FFT.
 */
class FFTProcessor {
  public:
    explicit FFTProcessor(int fftOrder = 14)
        : attackCoefficient(0.0f), decayCoefficient(0.0f), frequencySmoothingStrength(0.3f) {
        setFFTOrder(fftOrder);
    }

    void setFFTOrder(int order) {
        order = juce::jlimit(10, 15, order);
        if (order == currentFFTOrder)
            return;

        currentFFTOrder = order;
        fftSize = 1 << order;

        fft = std::make_unique<juce::dsp::FFT>(order);

        fftData.setSize(2, fftSize * 2, false, true, true);
        window.setSize(1, fftSize, false, true, true);
        magnitudeSpectrum.setSize(1, fftSize / 2, false, true, true);
        smoothedMagnitudeSpectrum.setSize(1, fftSize / 2, false, true, true);

        monoRingBuffer.setSize(1, fftSize, false, true, true);
        monoRingBuffer.clear();
        monoWritePos = 0;
        monoBufferFilled = false;

        auto* windowData = window.getWritePointer(0);
        for (int i = 0; i < fftSize; ++i) {
            windowData[i] =
                0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi *
                                        static_cast<float>(i) / static_cast<float>(fftSize - 1)));
        }

        magnitudeSpectrum.clear();
        smoothedMagnitudeSpectrum.clear();
    }

    void setTemporalSmoothing(float attack, float decay) {
        attackCoefficient = juce::jlimit(0.0f, 1.0f, attack);
        decayCoefficient = juce::jlimit(0.0f, 1.0f, decay);
    }

    void setFrequencySmoothing(float strength) {
        frequencySmoothingStrength = juce::jlimit(0.0f, 1.0f, strength);
    }

    bool process(AudioSampleFifo<2>& fifo) {
        const int available = fifo.getNumAvailable();
        if (available <= 0)
            return monoBufferFilled;

        const int toRead = juce::jmin(available, fftSize);
        juce::AudioBuffer<float> tempBuffer(2, toRead);
        float* channelPointers[2] = {tempBuffer.getWritePointer(0), tempBuffer.getWritePointer(1)};

        const int samplesRead = fifo.pull(channelPointers, toRead);
        if (samplesRead <= 0)
            return monoBufferFilled;

        auto* ringData = monoRingBuffer.getWritePointer(0);
        const auto* left = tempBuffer.getReadPointer(0);
        const auto* right = tempBuffer.getReadPointer(1);

        for (int i = 0; i < samplesRead; ++i) {
            ringData[monoWritePos] = (left[i] + right[i]) * 0.5f;
            monoWritePos = (monoWritePos + 1) % fftSize;
        }

        if (!monoBufferFilled) {
            monoSamplesAccumulated += samplesRead;
            if (monoSamplesAccumulated >= fftSize)
                monoBufferFilled = true;
            else
                return false;
        }

        auto* fftInput = fftData.getWritePointer(0);
        const auto* windowData = window.getReadPointer(0);

        for (int i = 0; i < fftSize; ++i) {
            const int ringIdx = (monoWritePos + i) % fftSize;
            fftInput[i] = ringData[ringIdx] * windowData[i];
        }

        for (int i = fftSize; i < fftSize * 2; ++i) {
            fftInput[i] = 0.0f;
        }

        fft->performFrequencyOnlyForwardTransform(fftInput);

        auto* magnitudes = magnitudeSpectrum.getWritePointer(0);
        auto* smoothed = smoothedMagnitudeSpectrum.getWritePointer(0);
        const int numBins = fftSize / 2;

        for (int i = 0; i < numBins; ++i) {
            float newMagnitude = fftInput[i] * 4.0f / static_cast<float>(fftSize);

            if (newMagnitude > smoothed[i]) {
                smoothed[i] = smoothed[i] * attackCoefficient + newMagnitude * (1.0f - attackCoefficient);
            } else {
                smoothed[i] = smoothed[i] * decayCoefficient + newMagnitude * (1.0f - decayCoefficient);
            }

            magnitudes[i] = smoothed[i];
        }

        if (frequencySmoothingStrength > 0.0f) {
            for (int i = 1; i < numBins - 1; ++i) {
                float leftBin = magnitudes[i - 1];
                float centerBin = magnitudes[i];
                float rightBin = magnitudes[i + 1];

                float smoothed_val = (leftBin + centerBin * 2.0f + rightBin) * 0.25f;

                magnitudes[i] = centerBin * (1.0f - frequencySmoothingStrength) +
                                smoothed_val * frequencySmoothingStrength;
            }
        }

        return true;
    }

    const float* getMagnitudeSpectrum() const {
        return magnitudeSpectrum.getReadPointer(0);
    }

    int getNumBins() const { return fftSize / 2; }
    int getFFTSize() const { return fftSize; }
    int getFFTOrder() const { return currentFFTOrder; }

    float getBinFrequency(int bin, float sampleRate) const {
        return (static_cast<float>(bin) * sampleRate) / static_cast<float>(fftSize);
    }

  private:
    std::unique_ptr<juce::dsp::FFT> fft;
    int currentFFTOrder = 0;
    int fftSize = 0;

    float attackCoefficient;
    float decayCoefficient;
    float frequencySmoothingStrength;

    juce::AudioBuffer<float> fftData;
    juce::AudioBuffer<float> window;
    juce::AudioBuffer<float> magnitudeSpectrum;
    juce::AudioBuffer<float> smoothedMagnitudeSpectrum;
    juce::AudioBuffer<float> monoRingBuffer;
    int monoWritePos = 0;
    bool monoBufferFilled = false;
    int monoSamplesAccumulated = 0;
};

} // namespace audio
} // namespace phu
