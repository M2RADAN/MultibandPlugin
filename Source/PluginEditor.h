#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h" // ¬ключает объ€вление MBRPAudioProcessor

//==============================================================================
/**
     ласс редактора дл€ плагина MBRP
*/
class MBRPAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    //  онструктор принимает ссылку на процессор MBRPAudioProcessor
    explicit MBRPAudioProcessorEditor(MBRPAudioProcessor&);
    ~MBRPAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // —сылка на наш аудио процессор
    MBRPAudioProcessor& processorRef;

    // Ёлементы управлени€ UI
    juce::Slider lowMidCrossoverSlider;
    juce::Slider midHighCrossoverSlider;
    juce::Slider lowPanSlider;
    juce::Slider midPanSlider;
    juce::Slider highPanSlider;

    // ћетки дл€ элементов управлени€
    juce::Label lowMidCrossoverLabel;
    juce::Label midHighCrossoverLabel;
    juce::Label lowPanLabel;
    juce::Label midPanLabel;
    juce::Label highPanLabel;

    // Attachments дл€ автоматической св€зи UI с параметрами процессора
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;

    SliderAttachment lowMidCrossoverAttachment;
    SliderAttachment midHighCrossoverAttachment;
    SliderAttachment lowPanAttachment;
    SliderAttachment midPanAttachment;
    SliderAttachment highPanAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};