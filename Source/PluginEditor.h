#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

// Включаем компоненты GUI
#include "GUI/BandSelectControls.h"
#include "GUI/CustomButtons.h"
#include "GUI/LookAndFeel.h"
#include "GUI/RotarySliderWithLabels.h"
// Путь к НОВОМУ анализатору
#include "GUI/SpectrumAnalyzer/SpectrumAnalyzer.h"
#include "GUI/AnlyzerOverlay/AnalyzerOverlay.h" 

// ControlBar
struct ControlBar : juce::Component
{
    ControlBar();
    void resized() override;
    std::function<void(bool isAnalyzerOn)> onAnalyzerToggle;
    AnalyzerButton analyzerButton;
private:
    // Добавляем обработчик для кнопки внутри ControlBar
    void analyzerButtonToggled();
};

    // Добавляем обработчик для кнопки внутри ControlBar
    
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
//        juce::Rectangle<int> getAnalysisArea(juce::Rectangle<int> bounds) const;
//    };
//} // namespace MBRP_GUI

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
    void handleBandAreaClick(int bandIndex); // <-- Объявляем новый метод-обработчик
    void handleAnalyzerToggle(bool shouldBeOn);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};
