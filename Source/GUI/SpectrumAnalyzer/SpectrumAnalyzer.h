#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

//==============================================================================
class SpectrumAnalyzer : public juce::Component,
    private juce::Timer
{
public:
    SpectrumAnalyzer(MBRPAudioProcessor& p);
    ~SpectrumAnalyzer() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

private:
    MBRPAudioProcessor& audioProcessor;

    std::vector<float> spectrumData;
    // --- ДОБАВЛЕНО: Переменная для пикового уровня ---
    std::atomic<float> peakDbLevel{ -100.0f }; // Атомарная для безопасного доступа из paint
    // ---------------------------------------------

    void drawSpectrum(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void drawFrequencyMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds);

    float frequencyToX(float freq, float width) const;
    float xToFrequency(float x, float width) const;

    // --- ИЗМЕНЕНО: Диапазоны ---
    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float minDb = -100.0f;
    const float maxDb = 30.0f; // <-- Увеличено до +30 dB
    // --------------------------

    const juce::Colour overZeroDbColour{ juce::Colours::red };
    const juce::Colour spectrumLineColour{ juce::Colours::lightblue };
    const juce::Colour zeroDbLineColour{ juce::Colours::white.withAlpha(0.7f) };
    const juce::Colour gridLineColour{ juce::Colours::dimgrey.withAlpha(0.5f) };
    const juce::Colour textColour{ juce::Colours::lightgrey };


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};