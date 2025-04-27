// PluginEditor.h
#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

// --- ВКЛЮЧАЕМ КОМПОНЕНТЫ GUI ---
#include "GUI/BandSelectControls.h"
#include "GUI/CustomButtons.h"
#include "GUI/LookAndFeel.h"
#include "GUI/RotarySliderWithLabels.h"
#include "GUI/SpectrumAnalyzer/SpectrumAnalyzer.h" // Путь к НОВОМУ анализатору
// ---------------------------------------

// --- ControlBar (остается) ---
struct ControlBar : juce::Component
{
    ControlBar();
    void resized() override;
    AnalyzerButton analyzerButton;
};
// ----------------

// --- AnalyzerOverlay ---
namespace MBRP_GUI
{
    struct AnalyzerOverlay : juce::Component, juce::Timer
    {
        AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
            juce::AudioParameterFloat& midXover);
        void paint(juce::Graphics& g) override;
        void timerCallback() override;
        void resized() override;

    private: // <-- Убедитесь, что секция private существует
        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds);
        juce::AudioParameterFloat& lowMidXoverParam;
        juce::AudioParameterFloat& midHighXoverParam;

        // --- ДОБАВЛЕНО: Объявление переменных состояния ---
        float lastLowMidFreq;  // Хранит последнее значение для сравнения
        float lastMidHighFreq; // Хранит последнее значение для сравнения
        // -------------------------------------------------

        juce::Rectangle<int> getAnalysisArea(juce::Rectangle<int> bounds) const;
    };
} // конец namespace MBRP_GUI
// ---------------------------------


//==============================================================================
class MBRPAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer
{
public:
    explicit MBRPAudioProcessorEditor(MBRPAudioProcessor&);
    ~MBRPAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    LookAndFeel lnf;
    MBRPAudioProcessor& processorRef;

    // --- Компоненты GUI ---
    ControlBar controlBar;
    SpectrumAnalyzer analyzer;
    MBRP_GUI::AnalyzerOverlay analyzerOverlay; // Теперь использует объявление выше
    MBRP_GUI::BandSelectControls bandSelectControls;
    // ----------------------

    // Контролы панорамы/кроссовера (остаются)
    juce::Slider lowMidCrossoverSlider, midHighCrossoverSlider;
    juce::Label lowMidCrossoverLabel, midHighCrossoverLabel;
    RotarySliderWithLabels panSlider;
    juce::Label panLabel;
    // ---------------------------------------------------

    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;

    SliderAttachment lowMidCrossoverAttachment, midHighCrossoverAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;

    void updatePanAttachment(int bandIndex); // Остается

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};