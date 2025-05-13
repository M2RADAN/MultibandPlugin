#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

// Включаем компоненты GUI
#include "GUI/BandSelectControls.h"
#include "GUI/CustomButtons.h"
#include "GUI/LookAndFeel.h"
#include "GUI/RotarySliderWithLabels.h"
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
    void analyzerButtonToggled();
};

//==============================================================================
class MBRPAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer // Таймер может быть не нужен здесь, если нет активной анимации в редакторе
{
public:
    explicit MBRPAudioProcessorEditor(MBRPAudioProcessor&);
    ~MBRPAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override; // Если нужен

private:
    LookAndFeel lnf;
    MBRPAudioProcessor& processorRef;

    // Компоненты GUI
    ControlBar controlBar;
    MBRP_GUI::SpectrumAnalyzer analyzer;
    MBRP_GUI::AnalyzerOverlay analyzerOverlay; // Будет принимать 3 параметра кроссовера
    MBRP_GUI::BandSelectControls bandSelectControls; // Будет иметь 4 кнопки

    // Контролы кроссоверов (теперь 3)
    juce::Slider lowMidCrossoverSlider, midCrossoverSlider, midHighCrossoverSlider;
    juce::Label lowMidCrossoverLabel, midCrossoverLabel, midHighCrossoverLabel;

    // Контролы панорамы и Bypass
    RotarySliderWithLabels panSlider;
    juce::Label panLabel;
    PowerButton bypassButton;

    // Контролы реверба
    RotarySliderWithLabels wetSlider;
    RotarySliderWithLabels spaceSlider;
    RotarySliderWithLabels distanceSlider;
    RotarySliderWithLabels delaySlider;
    // juce::Label wetLabel, spaceLabel, distanceLabel, delayLabel; // Не используются с RotarySliderWithLabels

    RotarySliderWithLabels gainSlider; 
    juce::TextButton bandBypassButton, bandSoloButton, bandMuteButton;

    // Attachments
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;
    using ButtonAttachment = APVTS::ButtonAttachment;

    SliderAttachment lowMidCrossoverAttachment, midCrossoverAttachment, midHighCrossoverAttachment;
    std::unique_ptr<ButtonAttachment> globalBypassAttachment; 
    std::unique_ptr<SliderAttachment> panAttachment;
    std::unique_ptr<SliderAttachment> wetAttachment;
    std::unique_ptr<SliderAttachment> spaceAttachment;
    std::unique_ptr<SliderAttachment> distanceAttachment;
    std::unique_ptr<SliderAttachment> delayAttachment;

    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<ButtonAttachment> bandBypassAttachment;
    std::unique_ptr<ButtonAttachment> bandSoloAttachment;
    std::unique_ptr<ButtonAttachment> bandMuteAttachment;

    // Методы
    void updatePanAttachment(int bandIndex);     // bandIndex 0..3
    void updateReverbAttachments(int bandIndex); // bandIndex 0..3
    void updateBandSpecificControls(int bandIndex);
    void handleBandAreaClick(int bandIndex);     // bandIndex 0..3
    void handleAnalyzerToggle(bool shouldBeOn);

    int currentSelectedBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};
