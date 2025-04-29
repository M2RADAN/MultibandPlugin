#pragma once

#include <JuceHeader.h>
#include <functional> 
#include "../Source/GUI/LookAndFeel.h"

namespace juce { class AudioParameterFloat; }


namespace MBRP_GUI
{

    template<typename FloatType>
    static FloatType mapFreqToXLog(FloatType freq, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        freq = juce::jlimit(minF, maxF, freq);
        if (maxF / minF <= 1.0f || freq <= 0 || width <= 0) return left;
        return left + width * (std::log(freq / minF) / std::log(maxF / minF));
    }


    template<typename FloatType>
    static FloatType mapXToFreqLog(FloatType x, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        x = juce::jlimit(left, x, left + width); 
        if (width <= 0 || maxF / minF <= 1.0f) return minF;
        float proportion = (x - left) / width;
        return minF * std::exp(proportion * std::log(maxF / minF));
    }

    class AnalyzerOverlay final : public juce::Component, public juce::Timer
    {
    public:

        AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
            juce::AudioParameterFloat& midXover);
        ~AnalyzerOverlay() override = default; 


        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;

        void mouseMove(const juce::MouseEvent& event) override; // <-- Добавлено
        void mouseExit(const juce::MouseEvent& event) override; // <-- Добавлено
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        std::function<void(int)> onBandAreaClicked;

    private:

        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        void drawHoverHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        float xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const;

        juce::Rectangle<float> getGraphBounds() const;


        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;


        enum class DraggingState { None, DraggingLowMid, DraggingMidHigh };
        DraggingState currentDraggingState{ DraggingState::None };

        enum class HoverState { None, HoveringLowMid, HoveringMidHigh }; // <-- Добавлено
        HoverState currentHoverState{ HoverState::None };                // <-- Добавлено
        HoverState lastHoverStateForColor { HoverState::None };
        const float dragTolerance = 20.0f;
        const float highlightRectWidth = 20.0f;  // change appropriatly

        float currentHighlightAlpha = 0.0f;  // <-- Добавлено: Текущая прозрачность
        float targetHighlightAlpha = 0.0f;   // <-- Добавлено: Целевая прозрачность
        const float alphaAnimationSpeed = 0.2f; // <-- Добавлено: Скорость (0.0 до 1.0, чем больше, тем быстрее)
        const float targetAlphaValue = 0.35f; // <-- Добавлено: Максимальная альфа подсветки

        static constexpr float minLogFreq = 20.0f;
        static constexpr float maxLogFreq = 20000.0f;



        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerOverlay)
    };

} // namespace MBRP_GUI