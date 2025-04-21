#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MBRPAudioProcessorEditor::MBRPAudioProcessorEditor(MBRPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider),
    lowPanAttachment(processorRef.getAPVTS(), "lowPan", lowPanSlider),
    midPanAttachment(processorRef.getAPVTS(), "midPan", midPanSlider),
    highPanAttachment(processorRef.getAPVTS(), "highPan", highPanSlider)
{
    // Лямбда для настройки слайдеров и лейблов
    auto setupSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText,
        juce::Slider::SliderStyle style = juce::Slider::RotaryHorizontalVerticalDrag,
        juce::Colour colour = juce::Colours::skyblue)
        {
            slider.setSliderStyle(style);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow,
                true,
                (style == juce::Slider::LinearHorizontal ? 80 : 60),
                20);
            slider.setPopupDisplayEnabled(true, false, this);
            slider.setColour(juce::Slider::thumbColourId, colour);
            if (style == juce::Slider::RotaryHorizontalVerticalDrag)
                slider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::rotarySliderFillColourId, colour.withAlpha(0.7f));
            addAndMakeVisible(slider);

            // Используем labelText (который теперь будет английским)
            label.setText(labelText, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.attachToComponent(&slider, false);
            label.setFont(juce::Font(12.0f));
            label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(label);
        };

    // --- ИЗМЕНЕНЫ СТРОКИ НА АНГЛИЙСКИЙ ---
    setupSlider(lowMidCrossoverSlider, lowMidCrossoverLabel, "Low / Mid", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);
    setupSlider(midHighCrossoverSlider, midHighCrossoverLabel, "Mid / High", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);
    setupSlider(lowPanSlider, lowPanLabel, "Low Pan", juce::Slider::RotaryHorizontalVerticalDrag, juce::Colours::orange);
    setupSlider(midPanSlider, midPanLabel, "Mid Pan", juce::Slider::RotaryHorizontalVerticalDrag, juce::Colours::lightgreen);
    setupSlider(highPanSlider, highPanLabel, "High Pan", juce::Slider::RotaryHorizontalVerticalDrag, juce::Colours::cyan);
    // --------------------------------------

    setSize(500, 250);
}

MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor()
{
}

//==============================================================================
void MBRPAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(0.8f));
}

void MBRPAudioProcessorEditor::resized()
{
    int padding = 20;
    auto bounds = getLocalBounds().reduced(padding);

    int crossoverHeight = 50;
    auto crossoverArea = bounds.removeFromTop(crossoverHeight);
    int crossoverWidth = crossoverArea.getWidth() / 2;
    lowMidCrossoverSlider.setBounds(crossoverArea.removeFromLeft(crossoverWidth).reduced(padding / 2));
    midHighCrossoverSlider.setBounds(crossoverArea.reduced(padding / 2));

    bounds.removeFromTop(padding);
    auto panArea = bounds;
    int panWidth = panArea.getWidth() / 3;
    lowPanSlider.setBounds(panArea.removeFromLeft(panWidth).reduced(padding));
    midPanSlider.setBounds(panArea.removeFromLeft(panWidth).reduced(padding));
    highPanSlider.setBounds(panArea.reduced(padding));
}