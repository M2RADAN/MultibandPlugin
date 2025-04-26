#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h" // �������� MBRPAudioProcessor

// --- �������� ���������� GUI ---
// �������, ��� ���� � ������������ ���� �����, � ����� �����������
#include "GUI/LookAndFeel.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/CustomButtons.h"
#include "GUI/BandSelectControls.h"
#include "GUI/RotarySliderWithLabels.h"
// -------------------------------

// --- ControlBar ---
struct ControlBar : juce::Component
{
    ControlBar();
    void resized() override;
    AnalyzerButton analyzerButton; // ���������� ������������ ����
};
// ----------------

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
    LookAndFeel lnf; // ���������� ������������ ����
    MBRPAudioProcessor& processorRef;

    // --- ���������� GUI ---
    ControlBar controlBar;
    MBRP_GUI::SpectrumAnalyzer analyzer;
    MBRP_GUI::AnalyzerOverlay analyzerOverlay;
    MBRP_GUI::BandSelectControls bandSelectControls;
    // ----------------------

    // �������� ��������/����������
    juce::Slider lowMidCrossoverSlider, midHighCrossoverSlider;
    juce::Label lowMidCrossoverLabel, midHighCrossoverLabel;

    // --- ��������: ������ ���� ����� ��������� �������� ---
    RotarySliderWithLabels panSlider;
    juce::Label panLabel;
    // ---------------------------------------------------

    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;

    // ���������� ��� ����������� (��������)
    SliderAttachment lowMidCrossoverAttachment, midHighCrossoverAttachment;

    // --- ��������: ������ ���� ��������� ��� �������� (����� ���������������) ---
    std::unique_ptr<SliderAttachment> panAttachment; // ���������� unique_ptr ��� ������� ������������
    // ----------------------------------------------------------------------

    // --- ���������: ������� ��� ���������� ���������� �������� ---
    void updatePanAttachment(int bandIndex);
    // ---------------------------------------------------------

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};