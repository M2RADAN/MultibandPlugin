/*
  ==============================================================================

    SpectrumAnalyzer.h
    (Путь: GUI/SpectrumAnalyzer.h)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Utilities.h" // <-- ВКЛЮЧАЕМ УТИЛИТЫ

// --- Включаем остальные зависимости ПОСЛЕ Utilities ---
#include "../PluginProcessor.h" // Нужен для MBRPAudioProcessor::SimpleFifo
#include "PathProducer.h"
// --------------------------------------------------

// --- Переносим сюда определение констант (если они не в Utilities.h) ---
// Или убедись, что Utilities.h включен ПЕРЕД этим файлом в PluginEditor.h
// и содержит эти константы в пространстве имен MBRP_GUI
namespace MBRP_GUI
{
    // --- ВОЗВРАЩАЕМ ОБЪЯВЛЕНИЕ СТРУКТУРЫ ---
    struct SpectrumAnalyzerUtils
    {
        // Только объявления статических методов
        static juce::Rectangle<int> getRenderArea(juce::Rectangle<int> bounds);
        static juce::Rectangle<int> getAnalysisArea(juce::Rectangle<int> bounds);
    };
    // УБРАЛИ ОПРЕДЕЛЕНИЯ КОНСТАНТ И MAPX/MAPY ОТСЮДА
    // УБРАЛИ ОПРЕДЕЛЕНИЕ SpectrumAnalyzerUtils ОТСЮДА

    struct SpectrumAnalyzer : juce::Component, juce::Timer
    {
        // Используем полное имя типа из PluginProcessor.h
        using SimpleFifo = MBRPAudioProcessor::SimpleFifo;
        SpectrumAnalyzer(MBRPAudioProcessor& processor, SimpleFifo& left, SimpleFifo& right);
        ~SpectrumAnalyzer() override = default;

        void timerCallback() override;
        void paint(juce::Graphics& g) override;
        void resized() override;

        void toggleAnalysisEnablement(bool enabled) { shouldShowFFTAnalysis = enabled; }

    private:
        MBRPAudioProcessor& audioProcessor;
        double sampleRate;
        bool shouldShowFFTAnalysis = true;

        // Используем функции и константы из Utilities.h (они видны здесь)
        void drawBackgroundGrid(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawTextLabels(juce::Graphics& g, juce::Rectangle<int> bounds);
        std::vector<float> getFrequencies();
        std::vector<float> getGains();
        std::vector<float> getXs(const std::vector<float>& freqs, float left, float width);

        PathProducer<juce::AudioBuffer<float>> leftPathProducer, rightPathProducer;

        void drawFFTAnalysis(juce::Graphics& g, juce::Rectangle<int> bounds);
    };

    struct AnalyzerOverlay : juce::Component, juce::Timer
    {
        AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
            juce::AudioParameterFloat& midXover);
        void paint(juce::Graphics& g) override;
        void timerCallback() override;

    private:
        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds);
        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;
    };

} //end namespace MBRP_GUI