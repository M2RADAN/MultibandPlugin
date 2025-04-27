#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h" // Путь к вашему процессору

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

    private:
        MBRPAudioProcessor& processor;

        // --- Данные для отображения ---
        std::vector<float> displayData;      // Данные для основной кривой (с EMA)
        std::vector<float> peakHoldLevels;   // Данные для линии пиков (с decay)
        std::atomic<float> peakDbLevel{ mindB }; // Пик всего спектра (мгновенный)

        // --- Функции отрисовки (хелперы) ---
        void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawGainScale(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds);
        void drawFrequencyMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds);

        // --- Вспомогательные функции ---
        float frequencyToX(float freq, float width) const;

        // --- Константы ---
        static constexpr float minFreq = 20.0f;
        static constexpr float maxFreq = 20000.0f;
        static constexpr float mindB = -100.0f;
        static constexpr float maxdB = 30.0f;

        // --- Коэффициенты ---
        static constexpr float smoothingAlpha = 0.2f; // Коэффициент сглаживания EMA
        static constexpr float peakHoldDecayFactor = 0.957f; // Затухание пиков

        // --- Цвета ---
        const juce::Colour backgroundColour{ juce::Colours::black };
        const juce::Colour spectrumFillColour{ juce::Colours::lightblue.withAlpha(0.2f) };
        const juce::Colour spectrumLineColour{ juce::Colours::lightblue };
        const juce::Colour peakHoldLineColour{ juce::Colours::lightgoldenrodyellow.withAlpha(0.7f) };
        // const juce::Colour overZeroDbFillColour   { juce::Colours::red.withAlpha(0.3f) }; // Пока не используется
        const juce::Colour overZeroDbLineColour{ juce::Colours::red };
        const juce::Colour zeroDbLineColour{ juce::Colours::white.withAlpha(0.5f) };
        const juce::Colour gridLineColour{ juce::Colours::dimgrey.withAlpha(0.3f) };
        const juce::Colour gridTextColour{ juce::Colours::lightgrey.withAlpha(0.7f) };
        const juce::Colour peakTextColour{ juce::Colours::white };


        // === ДОБАВЛЕНЫ НЕДОСТАЮЩИЕ ЧЛЕНЫ ИЗ witte/EqEditor ===
        int resizeDebounceInFrames = 0; // Задержка пересчета после ресайза

        // Объекты FFT (используем порядок из процессора)
        juce::dsp::FFT fftInput{ MBRPAudioProcessor::fftOrder };
        juce::dsp::FFT fftOutput{ MBRPAudioProcessor::fftOrder }; // Для выходного сигнала (если нужен)

        // Окно Ханна (используем размер из процессора)
        juce::dsp::WindowingFunction<float> hannWindow{ static_cast<size_t>(MBRPAudioProcessor::fftSize),
            juce::dsp::WindowingFunction<float>::hann };

        // Временные буферы для FFT
        juce::AudioBuffer<float> fftBufferInput{ 1, MBRPAudioProcessor::fftSize * 2 };
        juce::AudioBuffer<float> fftBufferOutput{ 1, MBRPAudioProcessor::fftSize * 2 }; // Для выходного

        // Буферы для скользящего среднего (усреднение по 4 кадрам + 1 для суммы)
        juce::AudioBuffer<float> avgInput{ 5, MBRPAudioProcessor::fftSize / 2 };
        juce::AudioBuffer<float> avgOutput{ 5, MBRPAudioProcessor::fftSize / 2 }; // Для выходного
        int avgInputPtr = 1; // Указатель на текущий слот усреднения (1..N-1)
        int avgOutputPtr = 1; // Для выходного

        // Блокировка для безопасного доступа к путям из разных потоков
        juce::CriticalSection pathCreationLock;
        // Пути убраны, т.к. формируются локально в paint/drawSpectrumAndPeaks

        // Структура для связи бинов FFT с точками X на экране
        struct fftPoint
        {
            int firstBinIndex = 0;
            int lastBinIndex = 1;
            int x = 0;
        };
        int fftPointsSize = 0; // Количество актуальных точек в fftPoints
        std::vector<fftPoint> fftPoints; // Вектор точек экрана

        // Объявление статической функции получения уровня для точки
        static float getFftPointLevel(const float* averagedMagnitudes, const fftPoint& point);

        // Объявление приватных методов
        void recalculateFftPoints();
        void drawNextFrame(); // Теперь это внутренний метод, вызываемый из timerCallback
        // =========================================================


        // Вспомогательная функция для текста
        static float getTextLayoutWidth(const juce::String& text, const juce::Font& font);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
    };

} // namespace MBRP_GUI