#pragma once

#include <JuceHeader.h>
#include <functional> 
#include "../Source/GUI/LookAndFeel.h" 
#include "../Source/PluginProcessor.h" 

namespace MBRP_GUI
{
    // Функции маппинга (оставляем здесь или переносим в Utilities, если используются еще где-то)
    template<typename FloatType>
    static FloatType mapFreqToXLog(FloatType freq, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        freq = juce::jlimit(minF, maxF, freq);
        if (minF <= 0 || maxF <= 0 || minF >= maxF || freq <= 0 || width <= 0) return left;
        return left + width * (std::log(freq / minF) / std::log(maxF / minF));
    }

    template<typename FloatType>
    static FloatType mapXToFreqLog(FloatType x, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        x = juce::jlimit(left, left + width, x);
        if (width <= 0 || minF <= 0 || maxF <= 0 || minF >= maxF) return minF;
        float proportion = (x - left) / width;
        proportion = juce::jlimit(0.0f, 1.0f, proportion);
        return minF * std::exp(proportion * std::log(maxF / minF));
    }

    template<typename FloatType>
    static FloatType mapGainDbToY(FloatType dbValue, FloatType top, FloatType height, FloatType minDbGUI, FloatType maxDbGUI) {
        if (height <= 0 || maxDbGUI <= minDbGUI) return top + height / 2.0f;
        float proportion = (dbValue - minDbGUI) / (maxDbGUI - minDbGUI);
        return top + height * (1.0f - juce::jlimit(0.0f, 1.0f, proportion));
    }

    template<typename FloatType>
    static FloatType mapYToGainDb(FloatType y, FloatType top, FloatType height, FloatType minDbGUI, FloatType maxDbGUI) {
        if (height <= 0 || maxDbGUI <= minDbGUI) return (minDbGUI + maxDbGUI) / 2.0f;
        float proportion = (y - top) / height;
        return minDbGUI + (1.0f - juce::jlimit(0.0f, 1.0f, proportion)) * (maxDbGUI - minDbGUI);
    }

    class AnalyzerOverlay final : public juce::Component, public juce::Timer
    {
    public:
        AnalyzerOverlay(MBRPAudioProcessor& p);
        ~AnalyzerOverlay() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;

        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        std::function<void(int bandIndex)> onBandAreaClicked;
        void setActiveBand(int bandIndex);
    private:
        // Объявление метода класса
        

        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        void drawHoverHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        void drawGainMarkersAndActiveBandHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds);

        float xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const;
        juce::Rectangle<float> getGraphBounds() const;

        MBRPAudioProcessor& processorRef;

        enum class CrossoverDraggingState { None, DraggingLowMid, DraggingMid, DraggingMidHigh };
        CrossoverDraggingState currentCrossoverDragState{ CrossoverDraggingState::None };
        enum class CrossoverHoverState { None, HoveringLowMid, HoveringMid, HoveringMidHigh };
        CrossoverHoverState currentCrossoverHoverState{ CrossoverHoverState::None };
        CrossoverHoverState lastCrossoverHoverStateForColor{ CrossoverHoverState::None };

        enum class GainDraggingState { None, DraggingLowGain, DraggingLowMidGain, DraggingMidHighGain, DraggingHighGain };
        GainDraggingState currentGainDragState{ GainDraggingState::None };
        std::pair<GainDraggingState, juce::AudioParameterFloat*> getGainInfoAt(const juce::MouseEvent& event, const juce::Rectangle<float>& graphBounds);

        int activeBandIndex{ 0 };

        const float dragToleranceX = 8.0f;
        const float dragToleranceY = 10.0f;
        const float highlightRectWidth = 8.0f;

        float currentHighlightAlpha = 0.0f;
        float targetHighlightAlpha = 0.0f;
        const float alphaAnimationSpeed = 0.2f;
        const float targetAlphaValue = 0.35f;

        // Эти константы могут быть static constexpr, если они не меняются для экземпляров
        static constexpr float minLogFreq = 20.0f;
        static constexpr float maxLogFreq = 20000.0f;

        const float gainMarkerMinDbOnGui = -96.0f;
        const float gainMarkerMaxDbOnGui = 36.0f;

        // --- ЧЛЕНЫ для Pop-up Display 
        juce::Label gainPopupDisplay;
        GainDraggingState currentlyHoveredOrDraggedGainState{ GainDraggingState::None };
        juce::AudioParameterFloat* currentlyHoveredOrDraggedGainParam{ nullptr };

        // Счетчик для задержки скрытия pop-up (в кадрах таймера)
        int popupHideDelayFramesCounter{ 0 };
        static const int popupHideDelayFrames = 60; // ~1 секунда при 60 Гц таймера, или 2 секунды при 30 Гц
        // Если таймер 30Гц, то 30 кадров = 1 сек, 60 кадров = 2 сек.
        // Если таймер 60Гц, то 60 кадров = 1 сек.
        // У нас startTimerHz(30) в конструкторе, так что 30 кадров = 1 сек.
        // Давайте сделаем задержку в 1 секунду (30 кадров)
        static const int POPUP_HIDE_DELAY_TOTAL_FRAMES = 30;


        void showGainPopup(const juce::MouseEvent* eventForPosition, float valueDb); // Передаем MouseEvent для позиции
        void hideGainPopup();
        void startPopupHideDelay();


        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerOverlay)
    };
}
