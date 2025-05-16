#pragma once

#include <JuceHeader.h>
#include <functional> 
#include "../Source/GUI/LookAndFeel.h" // Для ColorScheme

namespace juce { class AudioParameterFloat; }

namespace MBRP_GUI
{
    // Функции маппинга (оставляем здесь или переносим в Utilities, если используются еще где-то)
    template<typename FloatType>
    static FloatType mapFreqToXLog(FloatType freq, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        freq = juce::jlimit(minF, maxF, freq); // Ограничиваем частоту диапазоном
        // Проверка на допустимые значения для логарифмирования
        if (minF <= 0 || maxF <= 0 || minF >= maxF || freq <= 0 || width <= 0) return left;
        return left + width * (std::log(freq / minF) / std::log(maxF / minF));
    }

    template<typename FloatType>
    static FloatType mapXToFreqLog(FloatType x, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        // Ограничиваем x границами области рисования
        x = juce::jlimit(left, left + width, x);
        if (width <= 0 || minF <= 0 || maxF <= 0 || minF >= maxF) return minF;
        float proportion = (x - left) / width;
        // Убедимся, что proportion не выходит за [0, 1] из-за ошибок округления
        proportion = juce::jlimit(0.0f, 1.0f, proportion);
        return minF * std::exp(proportion * std::log(maxF / minF));
    }

    class AnalyzerOverlay final : public juce::Component, public juce::Timer
    {
    public:
        // Конструктор теперь принимает 3 параметра кроссовера
        AnalyzerOverlay(juce::AudioParameterFloat& lowMidXover,
            juce::AudioParameterFloat& midXover,
            juce::AudioParameterFloat& midHighXover);
        ~AnalyzerOverlay() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;

        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        std::function<void(int bandIndex)> onBandAreaClicked; // bandIndex 0..3

    private:
        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        void drawHoverHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        float xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const;
        juce::Rectangle<float> getGraphBounds() const;

        // Три параметра кроссовера
        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;

        enum class DraggingState { None, DraggingLowMid, DraggingMid, DraggingMidHigh };
        DraggingState currentDraggingState{ DraggingState::None };

        enum class HoverState { None, HoveringLowMid, HoveringMid, HoveringMidHigh };
        HoverState currentHoverState{ HoverState::None };
        HoverState lastHoverStateForColor{ HoverState::None };

        const float dragTolerance = 8.0f;
        const float highlightRectWidth = 8.0f;

        float currentHighlightAlpha = 0.0f;
        float targetHighlightAlpha = 0.0f;
        const float alphaAnimationSpeed = 0.2f;
        const float targetAlphaValue = 0.35f;

        static constexpr float minLogFreq = 20.0f;
        static constexpr float maxLogFreq = 20000.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerOverlay)
    };
}
