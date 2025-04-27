#pragma once

#include <JuceHeader.h>
#include <functional> // Äëÿ std::function

// Ïðÿìîå îáúÿâëåíèå, ÷òîáû íå âêëþ÷àòü âåñü PluginProcessor.h â çàãîëîâîê
namespace juce { class AudioParameterFloat; }

// Ïîìåùàåì âñïîìîãàòåëüíûå ôóíêöèè ìàïïèíãà â ïðîñòðàíñòâî èìåí
// (Èëè ìîæíî âûíåñòè èõ â îòäåëüíûé ôàéë Utilities.h, åñëè îíè èñïîëüçóþòñÿ ãäå-òî åùå)
namespace MBRP_GUI
{
    // Ïðåîáðàçîâàíèå ×àñòîòà -> X (Ëîãàðèôìè÷åñêîå)
    template<typename FloatType>
    static FloatType mapFreqToXLog(FloatType freq, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        freq = juce::jlimit(minF, maxF, freq);
        if (maxF / minF <= 1.0f || freq <= 0 || width <= 0) return left;
        return left + width * (std::log(freq / minF) / std::log(maxF / minF));
    }

    // Ïðåîáðàçîâàíèå X -> ×àñòîòà (Ëîãàðèôìè÷åñêîå)
    template<typename FloatType>
    static FloatType mapXToFreqLog(FloatType x, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
        x = juce::jlimit(left, x, left + width); // Îãðàíè÷èâàåì X
        if (width <= 0 || maxF / minF <= 1.0f) return minF;
        float proportion = (x - left) / width;
        return minF * std::exp(proportion * std::log(maxF / minF));
    }

    // --- Êëàññ Îâåðëåÿ Àíàëèçàòîðà ---
    class AnalyzerOverlay final : public juce::Component, public juce::Timer
    {
    public:
        // Êîíñòðóêòîð ïðèíèìàåò ññûëêè íà ïàðàìåòðû êðîññîâåðà
        AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
            juce::AudioParameterFloat& midXover);
        ~AnalyzerOverlay() override = default; // Äåñòðóêòîð ïî óìîë÷àíèþ

        // --- Îñíîâíûå ìåòîäû Component/Timer ---
        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;

        // --- Îáðàáîò÷èêè ñîáûòèé ìûøè ---
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

        // --- Êîëáýê äëÿ óâåäîìëåíèÿ ðåäàêòîðà î êëèêå íà îáëàñòü ïîëîñû ---
        std::function<void(int)> onBandAreaClicked;

    private:
        // --- Ïðèâàòíûå ìåòîäû ---
        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds);
        // Ïðåîáðàçîâàíèå X â ÷àñòîòó (èñïîëüçóåò êîíñòàíòû min/maxFreq)
        float xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const;
        // Ïîëó÷åíèå îáëàñòè ÃÐÀÔÈÊÀ àíàëèçàòîðà (âàæíî äëÿ êîîðäèíàò ìûøè)
        juce::Rectangle<float> getGraphBounds() const;

        // --- Ññûëêè íà ïàðàìåòðû ïðîöåññîðà ---
        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;

        // --- Ñîñòîÿíèå ïåðåòàñêèâàíèÿ ---
        enum class DraggingState { None, DraggingLowMid, DraggingMidHigh };
        DraggingState currentDraggingState{ DraggingState::None };
        const float dragTolerance = 5.0f; // Äîïóñê â ïèêñåëÿõ äëÿ çàõâàòà ëèíèè

        // --- Êîíñòàíòû äëÿ ìàïïèíãà (ìîæíî âûíåñòè) ---
        static constexpr float minLogFreq = 20.0f;
        static constexpr float maxLogFreq = 20000.0f;

        // --- Öâåòà ëèíèé êðîññîâåðà ---
        const juce::Colour crossoverLineColour{ juce::Colours::orange };
        const juce::Colour crossoverLineColour2{ juce::Colours::cyan };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerOverlay)
    };

} // namespace MBRP_GUI