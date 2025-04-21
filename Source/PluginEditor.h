#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h" // �������� ���������� MBRPAudioProcessor

//==============================================================================
/**
    ����� ��������� ��� ������� MBRP
*/
class MBRPAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    // ����������� ��������� ������ �� ��������� MBRPAudioProcessor
    explicit MBRPAudioProcessorEditor(MBRPAudioProcessor&);
    ~MBRPAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // ������ �� ��� ����� ���������
    MBRPAudioProcessor& processorRef;

    // �������� ���������� UI
    juce::Slider lowMidCrossoverSlider;
    juce::Slider midHighCrossoverSlider;
    juce::Slider lowPanSlider;
    juce::Slider midPanSlider;
    juce::Slider highPanSlider;

    // ����� ��� ��������� ����������
    juce::Label lowMidCrossoverLabel;
    juce::Label midHighCrossoverLabel;
    juce::Label lowPanLabel;
    juce::Label midPanLabel;
    juce::Label highPanLabel;

    // Attachments ��� �������������� ����� UI � ����������� ����������
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;

    SliderAttachment lowMidCrossoverAttachment;
    SliderAttachment midHighCrossoverAttachment;
    SliderAttachment lowPanAttachment;
    SliderAttachment midPanAttachment;
    SliderAttachment highPanAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};