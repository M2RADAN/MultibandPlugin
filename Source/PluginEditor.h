#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

// Включаем компоненты GUI
#include "GUI/LookAndFeel.h"         // LookAndFeel используется редактором
#include "GUI/CustomButtons.h"       // Используется в ControlBar
#include "GUI/BandSelectControls.h"  // Член редактора
#include "GUI/RotarySliderWithLabels.h"// Член редактора
#include "GUI/SpectrumAnalyzer/SpectrumAnalyzer.h" // Член редактора
#include "GUI/AnalyzerOverlay/AnalyzerOverlay.h"    // Член редактора <-- ВКЛЮЧАЕМ ЗДЕСЬ
// ControlBar
struct ControlBar : juce::Component
{
    ControlBar();
    void resized() override;
    AnalyzerButton analyzerButton;
};

// AnalyzerOverlay
//namespace MBRP_GUI
//{
//    struct AnalyzerOverlay : juce::Component, juce::Timer
//    {
//        AnalyzerOverlay(juce::AudioParameterFloat& lowXover, juce::AudioParameterFloat& midXover);
//        void paint(juce::Graphics& g) override;
//        void timerCallback() override;
//        void resized() override;
//
//    private:
//        void drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds);
//        juce::AudioParameterFloat& lowMidXoverParam;
//        juce::AudioParameterFloat& midHighXoverParam;
//        float lastLowMidFreq;
//        float lastMidHighFreq;
//    };
//} // namespace MBRP_GUI
//
////==============================================================================
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

    // Компоненты GUI
    ControlBar controlBar;
    MBRP_GUI::SpectrumAnalyzer analyzer; // Используем новый анализатор
    MBRP_GUI::AnalyzerOverlay analyzerOverlay;
    MBRP_GUI::BandSelectControls bandSelectControls;

    // Контролы
    juce::Slider lowMidCrossoverSlider, midHighCrossoverSlider;
    juce::Label lowMidCrossoverLabel, midHighCrossoverLabel;
    RotarySliderWithLabels panSlider;
    juce::Label panLabel;

    // Attachments
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;
    SliderAttachment lowMidCrossoverAttachment, midHighCrossoverAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;

    // Методы
    void updatePanAttachment(int bandIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};