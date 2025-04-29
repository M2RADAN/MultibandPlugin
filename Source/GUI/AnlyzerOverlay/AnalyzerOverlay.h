#pragma once

#include <JuceHeader.h>
#include <functional> 

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


        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        std::function<void(int)> onBandAreaClicked;

    private:

        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds);

        float xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const;

        juce::Rectangle<float> getGraphBounds() const;


        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;


        enum class DraggingState { None, DraggingLowMid, DraggingMidHigh };
        DraggingState currentDraggingState{ DraggingState::None };
        const float dragTolerance = 5.0f;

        static constexpr float minLogFreq = 20.0f;
        static constexpr float maxLogFreq = 20000.0f;


        const juce::Colour crossoverLineColour{ juce::Colours::orange };
        const juce::Colour crossoverLineColour2{ juce::Colours::cyan };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerOverlay)
    };

} // namespace MBRP_GUI