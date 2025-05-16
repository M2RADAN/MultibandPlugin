#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h" // Для MBRPAudioProcessor::fftSize и т.д.
#include "../Source/GUI/LookAndFeel.h" // Для ColorScheme

namespace MBRP_GUI
{

    class SpectrumAnalyzer final : public juce::Component, juce::Timer
    {
    public:
        explicit SpectrumAnalyzer(MBRPAudioProcessor& p);
        ~SpectrumAnalyzer() override = default;

        void paint(juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;

        void setAnalyzerActive(bool isActive);
        // bool isAnalyzerActive() const { return isVisible() && this->analyzerIsActive; } // Старый вариант
        bool isAnalyzerActive() const { return this->analyzerIsActive.load(); } // Проверяем только флаг

    private:
        MBRPAudioProcessor& processor;
        std::atomic<bool> analyzerIsActive{ true }; // По умолчанию активен

        std::vector<float> displayData;    // Данные для текущего отображения (сглаженные)
        std::vector<float> peakHoldLevels;   // Уровни удержания пиков
        std::atomic<float> peakDbLevel{ mindB }; // Общий пиковый уровень текущего кадра

        // Методы отрисовки
        void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawGainScale(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds);

        float frequencyToX(float freq, float width) const; // Преобразование частоты в X-координату

        // Константы для отображения
        static constexpr float minFreq = 20.0f;
        static constexpr float maxFreq = 20000.0f;
        static constexpr float mindB = -100.0f; // Минимальный уровень dB для отображения
        static constexpr float maxdB = 24.0f;  // Максимальный уровень dB для отображения // Изменено с 30 на 24 для соответствия шкале
        const float gainAdjustment = -40.0f; // Убрал старую подстройку, т.к. теперь это выходной сигнал

        // Параметры сглаживания и удержания пиков
        static constexpr float smoothingAlpha = 0.2f;
        static constexpr float peakHoldDecayFactor = 0.957f; // Коэффициент затухания пиков

        int resizeDebounceInFrames = 0; // Для задержки пересчета точек после изменения размера
        int lastWidthForFftPointsRecalc = 0;

        // --- ИЗМЕНЕНО: DSP для одного потока данных (выходного) ---
        juce::dsp::FFT forwardFFT{ MBRPAudioProcessor::fftOrder }; // Один объект FFT
        juce::dsp::WindowingFunction<float> hannWindow{ static_cast<size_t>(MBRPAudioProcessor::fftSize),
            juce::dsp::WindowingFunction<float>::hann };
        juce::AudioBuffer<float> fftBuffer{ 1, MBRPAudioProcessor::fftSize * 2 }; // Буфер для данных FFT
        juce::AudioBuffer<float> avgSpectrumData{ 5, MBRPAudioProcessor::fftSize / 2 }; // Буфер для усреднения магнитуд (5 кадров)
        int avgSpectrumDataPtr = 1; // Указатель для циклического буфера усреднения
        // ---------------------------------------------------------

        juce::CriticalSection pathCreationLock; // Для синхронизации доступа к avgSpectrumData

        // Структура для оптимизации отрисовки на логарифмической шкале
        struct fftPoint
        {
            int firstBinIndex = 0;
            int lastBinIndex = 1;
            int x = 0;
        };
        int fftPointsSize = 0;
        std::vector<fftPoint> fftPoints;

        // Вспомогательные методы
        static float getFftPointLevel(const float* averagedMagnitudes, const fftPoint& point); // Удалено, т.к. не используется в текущей версии
        void recalculateFftPoints(); // Пересчитывает fftPoints при изменении размера
        void drawNextFrame();        // Обрабатывает следующий блок данных из FIFO

        static float getTextLayoutWidth(const juce::String& text, const juce::Font& font); // Для расчета ширины текста

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
    };
}
