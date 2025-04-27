#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h" // Путь к нашему процессору

namespace MBRP_GUI
{

    class SpectrumAnalyzer final : public juce::Component, public juce::Timer
    {
    public:
        explicit SpectrumAnalyzer(MBRPAudioProcessor& p); // Только процессор
        ~SpectrumAnalyzer() override = default;

        void paint(juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;

    private:
        MBRPAudioProcessor& processor; // Ссылка на процессор

        // Данные для отображения
        std::vector<float> displayData;      // Основная линия (с EMA)
        std::vector<float> peakHoldLevels;   // Линия пиков (с decay)
        std::atomic<float> peakDbLevel{ mindB }; // Текстовый пик

        // Функции отрисовки
        void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawGainScale(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        // Маркеры теперь рисует оверлей
        // void drawFrequencyMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds);

        // Вспомогательные
        float frequencyToX(float freq, float width) const;
        static float getTextLayoutWidth(const juce::String& text, const juce::Font& font); // Оставляем

        // Константы
        static constexpr float minFreq = 20.0f; static constexpr float maxFreq = 20000.0f;
        static constexpr float mindB = -100.0f; static constexpr float maxdB = 30.0f;
        static constexpr float smoothingAlpha = 0.2f;
        static constexpr float peakHoldDecayFactor = 0.98f; // Сделаем чуть быстрее по умолчанию

        // Цвета
        const juce::Colour backgroundColour{ juce::Colours::black };
        const juce::Colour spectrumFillColour{ juce::Colours::lightblue.withAlpha(0.2f) };
        const juce::Colour spectrumLineColour{ juce::Colours::lightblue };
        const juce::Colour peakHoldLineColour{ juce::Colours::lightgoldenrodyellow.withAlpha(0.7f) };
        const juce::Colour overZeroDbLineColour{ juce::Colours::red };
        const juce::Colour zeroDbLineColour{ juce::Colours::white.withAlpha(0.5f) };
        const juce::Colour gridLineColour{ juce::Colours::dimgrey.withAlpha(0.3f) };
        const juce::Colour gridTextColour{ juce::Colours::lightgrey.withAlpha(0.7f) };
        const juce::Colour peakTextColour{ juce::Colours::white };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
    };

} // namespace MBRP_GUI