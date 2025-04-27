#pragma once

#include <JuceHeader.h>
#include <functional> // Для std::function

// Прямое объявление, чтобы не включать весь PluginProcessor.h в заголовок
namespace juce { class AudioParameterFloat; }

// Помещаем вспомогательные функции маппинга в пространство имен
// (Или можно вынести их в отдельный файл Utilities.h, если они используются где-то еще)
namespace MBRP_GUI
{
    // Преобразование Частота -> X (Логарифмическое)
    template<typename FloatType>
    static FloatType mapFreqToXLog(FloatType freq, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        freq = juce::jlimit(minF, maxF, freq);
        if (maxF / minF <= 1.0f || freq <= 0 || width <= 0) return left;
        return left + width * (std::log(freq / minF) / std::log(maxF / minF));
    }

    // Преобразование X -> Частота (Логарифмическое)
    template<typename FloatType>
    static FloatType mapXToFreqLog(FloatType x, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        x = juce::jlimit(left, x, left + width); // Ограничиваем X
        if (width <= 0 || maxF / minF <= 1.0f) return minF;
        float proportion = (x - left) / width;
        return minF * std::exp(proportion * std::log(maxF / minF));
    }

    // --- Класс Оверлея Анализатора ---
    class AnalyzerOverlay final : public juce::Component, public juce::Timer
    {
    public:
        // Конструктор принимает ссылки на параметры кроссовера
        AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
            juce::AudioParameterFloat& midXover);
        ~AnalyzerOverlay() override = default; // Деструктор по умолчанию

        // --- Основные методы Component/Timer ---
        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;

        // --- Обработчики событий мыши ---
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        // --- Колбэк для уведомления редактора о клике на область полосы ---
        std::function<void(int)> onBandAreaClicked;

    private:
        // --- Приватные методы ---
        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        // Преобразование X в частоту (использует константы min/maxFreq)
        float xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const;
        // Получение области ГРАФИКА анализатора (важно для координат мыши)
        juce::Rectangle<float> getGraphBounds() const;

        // --- Ссылки на параметры процессора ---
        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;

        // --- Состояние перетаскивания ---
        enum class DraggingState { None, DraggingLowMid, DraggingMidHigh };
        DraggingState currentDraggingState{ DraggingState::None };
        const float dragTolerance = 5.0f; // Допуск в пикселях для захвата линии

        // --- Константы для маппинга (можно вынести) ---
        static constexpr float minLogFreq = 20.0f;
        static constexpr float maxLogFreq = 20000.0f;

        // --- Цвета линий кроссовера ---
        const juce::Colour crossoverLineColour{ juce::Colours::orange };
        const juce::Colour crossoverLineColour2{ juce::Colours::cyan };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerOverlay)
    };

} // namespace MBRP_GUI