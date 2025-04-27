#include "PluginProcessor.h"
#include "PluginEditor.h" // �������� MBRPAudioProcessorEditor � ����������� GUI

// --- ���������� ControlBar ---
ControlBar::ControlBar()
{
    // ������������� ������ �����������
    analyzerButton.setToggleState(true, juce::NotificationType::dontSendNotification); // �������� �� ���������
    addAndMakeVisible(analyzerButton); // ��������� �� �����
}

void ControlBar::resized()
{
    // ������������ ������ ����������� �����
    auto bounds = getLocalBounds();
    analyzerButton.setBounds(bounds.removeFromLeft(50) // ������ ������
        .withTrimmedTop(4)      // ��������� ������ ������
        .withTrimmedLeft(4));   // ��������� ������ �����
}
// ---------------------------


//==============================================================================
MBRPAudioProcessorEditor::MBRPAudioProcessorEditor(MBRPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    // ������������� ����������� GUI
    // �������� ��������� � ������ �� FIFO � ����������
    analyzer(p, p.leftChannelFifo, p.rightChannelFifo),
    // �������� ������ �� ��������� ���������� � �������
    analyzerOverlay(*p.lowMidCrossover, *p.midHighCrossover),
    panSlider(nullptr, "", "Pan"),
    // ������������� Attachments ��� ��������� �����������
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider)
    // ��������� ��� �������� panAttachment ���������������� � ����� ������������
{
    setLookAndFeel(&lnf); // ������������� ��������� LookAndFeel

    // ���������� ���������� ��� ������ ���/���� �����������
    controlBar.analyzerButton.onClick = [this]()
        {
            analyzer.toggleAnalysisEnablement(controlBar.analyzerButton.getToggleState());
        };

    // ��������� ��� �������� ���������� GUI �� �����
    addAndMakeVisible(controlBar);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(analyzerOverlay); // Overlay �������� ������ �����������
    addAndMakeVisible(bandSelectControls); // ������ ������ ������

    // ������ ��� ��������� �������� ���� ���������
    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText,
        juce::Slider::SliderStyle style, juce::Colour colour)
        {
            slider.setSliderStyle(style);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20); // ��������� ��� ��������
            slider.setPopupDisplayEnabled(true, false, this);
            slider.setColour(juce::Slider::thumbColourId, colour);
            slider.setColour(juce::Slider::trackColourId, colour.darker(0.5f)); // ���� �����
            slider.setName(labelText);
            addAndMakeVisible(slider);

            label.setText(labelText, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.attachToComponent(&slider, false);
            label.setFont(juce::Font(12.0f));
            label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(label);
        };

    setupStandardSlider(lowMidCrossoverSlider, lowMidCrossoverLabel, "Low / Mid", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);
    setupStandardSlider(midHighCrossoverSlider, midHighCrossoverLabel, "Mid / High", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);

    panSlider.setPopupDisplayEnabled(true, false, this);
    // --- ��������: ��������� ������ ���������� �������� �������� ---
    // �������� ��������� ���� (�����, �����) ����� ����������� � updatePanAttachment
    // ����� ������ ������ ��� ������� � ����������� ������� �����
    addAndMakeVisible(panSlider); // ������ ������� RotarySliderWithLabels
    panLabel.setText("Pan", juce::dontSendNotification); // ��������� ����� ������
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.attachToComponent(&panSlider, false); // ����������� ������� ����� ��� ���
    panLabel.setFont(juce::Font(12.0f));
    panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(panLabel); // ������ ������� ������� �����
    // ---------------------------------------------------------------

    // ���������� ���������� ������ ������
    bandSelectControls.onBandSelected = [this](int bandIndex) {
        updatePanAttachment(bandIndex);
        };

    // �������������� ��������� � ��� �������� ��� ��������� ������ (Low)
    updatePanAttachment(0);

    setSize(600, 700);
    startTimerHz(60);
}


MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor()
{
    setLookAndFeel(nullptr); // ���������� LookAndFeel ��� �������� ���������
}

//==============================================================================
void MBRPAudioProcessorEditor::paint(juce::Graphics& g)
{
    // �������� ��� ������ �� LookAndFeel
    g.fillAll(lnf.findColour(juce::ResizableWindow::backgroundColourId));
    // ����� ����� �������� ��� ��� ��������� ���������� ��������� ��� ����,
    // ���������� ��� �� SimpleMBComp::paint � ����������� �����/������
}

void MBRPAudioProcessorEditor::resized()
{
    // ������������ ���� ����������� ������ ���� ���������
    auto bounds = getLocalBounds(); // ������� ����� ����
    int padding = 10; // ����������� ������

    // ControlBar ������
    controlBar.setBounds(bounds.removeFromTop(32));

    // ���������� ������� �������� ������� ����� ����������� ������������
    int analyzerHeight = 250; // ������ ������ �����������
    auto analyzerArea = bounds.removeFromTop(analyzerHeight);
    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzerArea); // ������� �������� ����� ��� ������������

    // ���������� ������� ��� ��������� ��������� � ���������
    auto controlsArea = bounds.reduced(padding);
    controlsArea.removeFromTop(padding); // �������������� ������ ������

    // �������� �����������
    int crossoverHeight = 50; // ������ ������� �����������
    auto crossoverArea = controlsArea.removeFromTop(crossoverHeight);
    int crossoverWidth = crossoverArea.getWidth() / 2; // ����� �������
    lowMidCrossoverSlider.setBounds(crossoverArea.removeFromLeft(crossoverWidth).reduced(padding / 2));
    midHighCrossoverSlider.setBounds(crossoverArea.reduced(padding / 2));

    controlsArea.removeFromTop(padding); // ������

    // ������ ������ ������
    int bandSelectHeight = 30; // ������ ������� ������
    bandSelectControls.setBounds(controlsArea.removeFromTop(bandSelectHeight));

    controlsArea.removeFromTop(padding / 2); // ������

    // ������������ ������� ��������
    auto panArea = controlsArea;
    // ��������� ���, ��������, �� ������ � ���������
    panSlider.setBounds(panArea.reduced(panArea.getWidth() / 4, padding));

    // ������ ������������� ����������� ��� ������ ����������
    // ��������� ������ label.attachToComponent(&slider, false);
}

void MBRPAudioProcessorEditor::timerCallback()
{
    // ���� ������ ����� �������������� ��� ���������� ��������� ��������� UI,
    // ������� �� ����������� ������������� (��������, RMS �����).
    // ���������� � Overlay ����� ���� ��������� ���������� ����� ������� ��� PathProducer.
    // ���� ��������� ������.
}

// --- ������� ���������� ���������� � ���� �������� �������� ---
void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex)
{
    juce::String paramId;       // ID ��������� �������� ��� ���� ������
    juce::String labelText;     // ����� ��� ������ ��� ���������
    juce::Colour color;         // ���� ��� ��������

    // ���������� ID, ����� � ���� � ����������� �� ��������� ������
    switch (bandIndex)
    {
    case 0: // Low
        paramId = "lowPan";
        labelText = "Low Pan";
        color = juce::Colours::orange;
        break;
    case 1: // Mid
        paramId = "midPan";
        labelText = "Mid Pan";
        color = juce::Colours::lightgreen;
        break;
    case 2: // High
        paramId = "highPan";
        labelText = "High Pan";
        color = juce::Colours::cyan;
        break;
    default:
        jassertfalse; // ������������ ������ ������
        return;
    }

    // --- ��������: �������� � RotarySliderWithLabels ---
   // 1. �������� ��������� �� ������� ��������
    auto* targetParam = processorRef.getAPVTS().getParameter(paramId);
    // ������������ ���������� ���� �� �����������, �.�. changeParam ��������� RangedAudioParameter
    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
    jassert(rangedParam != nullptr); // ��������, ��� �������� ������ � ����� ��������
    if (!rangedParam) return;

    // 2. ��������� ��������, � ������� �������� RotarySliderWithLabels
    //    � ��� ���������� �����/������� (������� �� ����� ��� ��������)
    panSlider.changeParam(rangedParam); // �������� ����� ��������
    panSlider.labels.clear();
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);// ������� ������ ����� (���� ����)
    // ��������� ����� ��� -1 (L), 0 (C), +1 (R)
    panSlider.labels.add({ 0.0f, "L" });   // ������� 0.0 ������������� �������� -1.0
    panSlider.labels.add({ 0.5f, "C" });   // ������� 0.5 ������������� �������� 0.0
    panSlider.labels.add({ 1.0f, "R" });   // ������� 1.0 ������������� �������� +1.0
    // 1. ������� ������ ��������� (���� �� ���)
    panAttachment.reset();
    // 2. ������� ����� ���������, �������� panSlider � ��������� paramId
    panAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramId, panSlider);

    // 3. ��������� ������� ��� �������� � ������
    panLabel.setText(labelText, juce::dontSendNotification);
    panSlider.setColour(juce::Slider::thumbColourId, color);
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, color.withAlpha(0.7f));
    panSlider.repaint(); // ������������� �������������� ������� � ����� ������
}
// ----------------------------------------------------------------------
