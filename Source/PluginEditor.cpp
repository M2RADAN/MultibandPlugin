#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- ControlBar Implementation ---
ControlBar::ControlBar() {
    analyzerButton.setClickingTogglesState(true); // Кнопка работает как переключатель
    analyzerButton.setTooltip("Enable/Disable Spectrum Analyzer");
    // Подключаем внутренний обработчик
    analyzerButton.onClick = [this] { analyzerButtonToggled(); };
    addAndMakeVisible(analyzerButton);
}
void ControlBar::resized() {
    auto bounds = getLocalBounds();
    // Размещаем кнопку, например, справа
    analyzerButton.setBounds(bounds.removeFromRight(30).reduced(2));
    // TODO: Äîáàâèòü ôóíêöèîíàë êíîïêå analyzerButton
}

void ControlBar::analyzerButtonToggled()
{
    // Если колбэк установлен, вызываем его с текущим состоянием кнопки
    if (onAnalyzerToggle) {
        onAnalyzerToggle(analyzerButton.getToggleState());
    }
}


//==============================================================================
MBRPAudioProcessorEditor::MBRPAudioProcessorEditor(MBRPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    analyzer(p), 
    analyzerOverlay(*p.lowMidCrossover, *p.midHighCrossover),
    panSlider(nullptr, "", "Pan"),
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider)
{
    setLookAndFeel(&lnf);
    processorRef.setCopyToFifo(true); 

    addAndMakeVisible(controlBar);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(analyzerOverlay);
    addAndMakeVisible(bandSelectControls);

    controlBar.onAnalyzerToggle = [this](bool state) { handleAnalyzerToggle(state); };
    bool initialAnalyzerState = processorRef.isCopyToFifoEnabled(); // Проверяем начальное состояние процессора
    controlBar.analyzerButton.setToggleState(initialAnalyzerState, juce::dontSendNotification);
    handleAnalyzerToggle(initialAnalyzerState); // Применяем начальное состояние



    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20);
        slider.setPopupDisplayEnabled(true, false, this);
        slider.setColour(juce::Slider::thumbColourId, ColorScheme::getSliderThumbColor());
        slider.setColour(juce::Slider::trackColourId, ColorScheme::getSliderTrackColor());
        // --- Óñòàíàâëèâàåì öâåò òåêñòà â TextBox ---
        slider.setColour(juce::Slider::textBoxTextColourId, ColorScheme::getSecondaryTextColor()); // Èñïîëüçóåì ìåíåå ÿðêèé ñåðûé öâåò
        // Óñòàíàâëèâàåì öâåò ðàìêè TextBox (ìîæíî ñäåëàòü ÷óòü òåìíåå ôîíà)
        slider.setColour(juce::Slider::textBoxOutlineColourId, ColorScheme::getSliderTrackColor().darker(0.2f));
        // Öâåò ôîíà TextBox ìîæíî îñòàâèòü ïî óìîë÷àíèþ èëè óñòàíîâèòü ÿâíî
        // slider.setColour(juce::Slider::textBoxBackgroundColourId, ColorScheme::getBackgroundColor().darker(0.05f));

        slider.setName(labelText);
        addAndMakeVisible(slider);
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f));
        label.setColour(juce::Label::textColourId, ColorScheme::getTextColor());
        addAndMakeVisible(label);
        };

    setupStandardSlider(lowMidCrossoverSlider, lowMidCrossoverLabel, "Low / Mid");
    setupStandardSlider(midHighCrossoverSlider, midHighCrossoverLabel, "Mid / High");
    panSlider.setPopupDisplayEnabled(true, false, this);
    // Óñòàíàâëèâàåì öâåòà ðîòîðíîãî ñëàéäåðà ÷åðåç LookAndFeel ID
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, ColorScheme::getSliderFillColor());
    panSlider.setColour(juce::Slider::rotarySliderOutlineColourId, ColorScheme::getSliderBorderColor());
    panSlider.setColour(juce::Slider::thumbColourId, ColorScheme::getSliderThumbColor()); // Öâåò óêàçàòåëÿ
    addAndMakeVisible(panSlider);

    panLabel.setText("Pan", juce::dontSendNotification); // Óñòàíàâëèâàåì òåêñò "Pan"
    panLabel.setJustificationType(juce::Justification::centred);
    // panLabel.attachToComponent(&panSlider, false); // Íå ïðèêðåïëÿåì, ðàçìåñòèì âðó÷íóþ
    panLabel.setFont(juce::Font(14.0f, juce::Font::bold)); // Äåëàåì ìåòêó "Pan" çàìåòíåå
    panLabel.setColour(juce::Label::textColourId, ColorScheme::getDarkTextColor()); // Èñïîëüçóåì áîëåå òåìíûé öâåò
    addAndMakeVisible(panLabel);

    bandSelectControls.onBandSelected = [this](int bandIndex) { updatePanAttachment(bandIndex); };
    analyzerOverlay.onBandAreaClicked = [this](int bandIndex) { handleBandAreaClick(bandIndex); }; // <-- Óñòàíîâëåí êîëáýê
    updatePanAttachment(0);

    setSize(900, 700);
    startTimerHz(30); 
}

MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor() {
    processorRef.setCopyToFifo(false); 
    setLookAndFeel(nullptr);
}

void MBRPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(ColorScheme::getBackgroundColor());
}

void MBRPAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    int padding = 10;
    int controlBarHeight = 32; // Óáðàë, åñëè ControlBar íå èñïîëüçóåòñÿ àêòèâíî
    controlBar.setBounds(bounds.removeFromTop(controlBarHeight).reduced(padding, 0));

    int analyzerHeight = 300; // Íåìíîãî óìåíüøèì àíàëèçàòîð
    auto analyzerArea = bounds.removeFromTop(analyzerHeight).reduced(padding, 0); // Äîáàâèì îòñòóïû ïî áîêàì
    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzerArea);

    // Îáëàñòü ïîä àíàëèçàòîðîì
    auto controlsArea = bounds.reduced(padding);
    controlsArea.removeFromTop(padding); // Îòñòóï ñâåðõó

    // Ñëàéäåðû êðîññîâåðîâ
    int labelHeight = 15;
    int sliderHeight = 40;
    int crossoverControlHeight = labelHeight + sliderHeight;
    auto crossoverArea = controlsArea.removeFromTop(crossoverControlHeight);
    int crossoverWidth = crossoverArea.getWidth() / 2 - padding / 2;

    auto lowMidArea = crossoverArea.removeFromLeft(crossoverWidth);
    lowMidCrossoverLabel.setBounds(lowMidArea.removeFromTop(labelHeight));
    lowMidCrossoverSlider.setBounds(lowMidArea);

    crossoverArea.removeFromLeft(padding); // Îòñòóï ìåæäó ñëàéäåðàìè

    auto midHighArea = crossoverArea;
    midHighCrossoverLabel.setBounds(midHighArea.removeFromTop(labelHeight));
    midHighCrossoverSlider.setBounds(midHighArea);

    controlsArea.removeFromTop(padding); // Îòñòóï

    // Êíîïêè âûáîðà ïîëîñû
    int bandSelectHeight = 30;
    bandSelectControls.setBounds(controlsArea.removeFromTop(bandSelectHeight).reduced(crossoverWidth / 2, 0)); // Öåíòðèðóåì êíîïêè

    controlsArea.removeFromTop(padding * 2); // Áîëüøèé îòñòóï ïåðåä Pan

    // Îáëàñòü Pan
    auto panArea = controlsArea;
    int panLabelHeight = 20; // Âûñîòà äëÿ ìåòêè "Pan"
    // Ðàçìåùàåì ìåòêó "Pan" ÂÍÈÇÓ îáëàñòè panArea
    panLabel.setBounds(panArea.removeFromBottom(panLabelHeight));
    // Îñòàâøàÿñÿ îáëàñòü äëÿ ñëàéäåðà, öåíòðèðóåì åãî
    panSlider.setBounds(panArea.reduced(panArea.getWidth() / 3, padding)); // Äåëàåì ñëàéäåð ïîêðóïíåå è öåíòðèðóåì



}

void MBRPAudioProcessorEditor::handleAnalyzerToggle(bool shouldBeOn)
{
    processorRef.setCopyToFifo(shouldBeOn); // Управляем копированием данных в FIFO
    analyzer.setAnalyzerActive(shouldBeOn); // Включаем/выключаем отрисовку в анализаторе
    DBG("Analyzer Toggled: " << (shouldBeOn ? "ON" : "OFF"));
}

void MBRPAudioProcessorEditor::timerCallback() {
   
}

void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex) {
    juce::String paramId;
    // juce::String labelText; // Áîëüøå íå íóæåí ñïåöèôè÷íûé òåêñò ìåòêè
    juce::Colour bandColour; // Öâåò äëÿ ñëàéäåðà

    switch (bandIndex) {
    case 0:
        paramId = "lowPan";
        bandColour = ColorScheme::getLowBandColor();
        break;
    case 1:
        paramId = "midPan";
        bandColour = ColorScheme::getMidBandColor();
        break;
    case 2:
        paramId = "highPan";
        bandColour = ColorScheme::getHighBandColor();
        break;
    default: jassertfalse; return;
    }

    auto* targetParam = processorRef.getAPVTS().getParameter(paramId);
    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
    jassert(rangedParam != nullptr); if (!rangedParam) return;

    panSlider.changeParam(rangedParam); // Îáíîâëÿåì ïàðàìåòð ñëàéäåðà

    // --- Óáèðàåì íàñòðîéêó òåêñòîâîãî ïîëÿ è âíóòðåííèõ ìåòîê ---
    panSlider.labels.clear(); // Î÷èùàåì ñòàðûå (íà âñÿêèé ñëó÷àé)
    panSlider.labels.add({ 0.0f, "L" }); // 0.0 ñîîòâåòñòâóåò çíà÷åíèþ -1.0 ïàíîðàìû
    panSlider.labels.add({ 0.5f, "C" }); // 0.5 ñîîòâåòñòâóåò çíà÷åíèþ 0.0 ïàíîðàìû
    panSlider.labels.add({ 1.0f, "R" }); // 1.0 ñîîòâåòñòâóåò çíà÷åíèþ +1.0 ïàíîðàìû
    // -----------------------------------------

    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // Òåêñòîâîå ïîëå âñå åùå íå íóæíî

    panAttachment.reset();
    panAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramId, panSlider);

    panLabel.setText("Pan", juce::dontSendNotification); // Âíåøíÿÿ ìåòêà îñòàåòñÿ "Pan"

    panSlider.setColour(juce::Slider::thumbColourId, bandColour);
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, bandColour.withAlpha(0.7f));
    panSlider.setColour(juce::Slider::rotarySliderOutlineColourId, bandColour.darker(0.3f));
    panSlider.repaint();
}

void MBRPAudioProcessorEditor::handleBandAreaClick(int bandIndex)
{
    DBG("Editor received band click from overlay: " << bandIndex);

    juce::ToggleButton* buttonToSelect = nullptr;
    switch (bandIndex) {
    case 0: buttonToSelect = &bandSelectControls.lowBandButton; break;
    case 1: buttonToSelect = &bandSelectControls.midBandButton; break;
    case 2: buttonToSelect = &bandSelectControls.highBandButton; break;
    default:
        jassertfalse; // Íåäîïóñòèìûé èíäåêñ
        return;
    }

    // Ïðîâåðÿåì, íå âûáðàíà ëè óæå ýòà êíîïêà
    if (buttonToSelect && !buttonToSelect->getToggleState())
    {
        // Âûçûâàåì setToggleState ñ óâåäîìëåíèåì.
        // Ýòî äîëæíî çàïóñòèòü onClick â BandSelectControls, êîòîðûé âûçîâåò onBandSelected,
        // êîòîðûé, â ñâîþ î÷åðåäü, âûçîâåò updatePanAttachment.
        // sendNotificationSync ãàðàíòèðóåò, ÷òî ýòî ïðîèçîéäåò íåìåäëåííî â ïîòîêå ñîîáùåíèé.
        buttonToSelect->setToggleState(true, juce::sendNotificationSync);

        // Íàïðÿìóþ âûçûâàòü updatePanAttachment çäåñü íå íóæíî,
        // òàê êàê ýòî ñäåëàåò êîëáýê onBandSelected èç bandSelectControls.
    }
}
