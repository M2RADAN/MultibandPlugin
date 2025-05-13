#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- ControlBar Implementation ---
ControlBar::ControlBar() {
    analyzerButton.setClickingTogglesState(true);
    analyzerButton.setTooltip("Enable/Disable Spectrum Analyzer");
    analyzerButton.onClick = [this] { analyzerButtonToggled(); };
    addAndMakeVisible(analyzerButton);
}
void ControlBar::resized() {
    auto bounds = getLocalBounds();
    analyzerButton.setBounds(bounds.removeFromRight(30).reduced(2));
}

void ControlBar::analyzerButtonToggled()
{
    if (onAnalyzerToggle) {
        onAnalyzerToggle(analyzerButton.getToggleState());
    }
}


//==============================================================================
MBRPAudioProcessorEditor::MBRPAudioProcessorEditor(MBRPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    analyzer(p),
    // Передаем все три параметра кроссовера в AnalyzerOverlay
    analyzerOverlay(*p.lowMidCrossover, *p.midCrossover, *p.midHighCrossover),
    wetSlider(nullptr, " %", "Wet"),
    spaceSlider(nullptr, " %", "Space"),
    distanceSlider(nullptr, " %", "Distance"),
    delaySlider(nullptr, " ms", "Pre-Delay"),
    panSlider(nullptr, "", "Pan"), // Суффикс не нужен, getDisplayString сам форматирует Pan
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midCrossoverAttachment(processorRef.getAPVTS(), "midCrossover", midCrossoverSlider), // Аттачмент для нового слайдера
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider)
{
    setLookAndFeel(&lnf);
    processorRef.setCopyToFifo(true);

    addAndMakeVisible(controlBar);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(analyzerOverlay);
    addAndMakeVisible(bandSelectControls); // Теперь с 4-мя кнопками

    auto setupReverbSliderComponent = [&](RotarySliderWithLabels& slider) {
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled(true, true, this, 500);
        addAndMakeVisible(slider);
    };

    setupReverbSliderComponent(wetSlider);
    setupReverbSliderComponent(spaceSlider);
    setupReverbSliderComponent(distanceSlider);
    setupReverbSliderComponent(delaySlider);

    controlBar.onAnalyzerToggle = [this](bool state) { handleAnalyzerToggle(state); };
    bool initialAnalyzerState = processorRef.isCopyToFifoEnabled();
    controlBar.analyzerButton.setToggleState(initialAnalyzerState, juce::dontSendNotification);
    handleAnalyzerToggle(initialAnalyzerState);

    bypassAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "bypass", bypassButton);
    bypassButton.setButtonText("Bypass");
    bypassButton.setTooltip("Bypass the plugin processing");
    bypassButton.setClickingTogglesState(true);
    addAndMakeVisible(bypassButton);


    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20);
        slider.setPopupDisplayEnabled(true, false, this);
        slider.setColour(juce::Slider::thumbColourId, ColorScheme::getSliderThumbColor());
        slider.setColour(juce::Slider::trackColourId, ColorScheme::getSliderTrackColor());
        slider.setColour(juce::Slider::textBoxTextColourId, ColorScheme::getSecondaryTextColor());
        slider.setColour(juce::Slider::textBoxOutlineColourId, ColorScheme::getSliderTrackColor().darker(0.2f));
        slider.setName(labelText); // Важно для идентификации в LookAndFeel или для автоматизации
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f));
        label.setColour(juce::Label::textColourId, ColorScheme::getTextColor());
        addAndMakeVisible(label);
    };

    setupStandardSlider(lowMidCrossoverSlider, lowMidCrossoverLabel, "Low / L-Mid");
    setupStandardSlider(midCrossoverSlider, midCrossoverLabel, "L-Mid / M-High"); // Новый слайдер
    setupStandardSlider(midHighCrossoverSlider, midHighCrossoverLabel, "M-High / High");

    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setPopupDisplayEnabled(true, true, this, 500);
    addAndMakeVisible(panSlider);

    panLabel.setText("Pan", juce::dontSendNotification);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    panLabel.setColour(juce::Label::textColourId, ColorScheme::getDarkTextColor());
    addAndMakeVisible(panLabel);

    bandSelectControls.onBandSelected = [this](int bandIndex) { // bandIndex 0..3
        updatePanAttachment(bandIndex);
        updateReverbAttachments(bandIndex);
    };
    analyzerOverlay.onBandAreaClicked = [this](int bandIndex) { handleBandAreaClick(bandIndex); }; // bandIndex 0..3

    updatePanAttachment(0);    // Инициализация для Low полосы
    updateReverbAttachments(0); // Инициализация для Low полосы

    setSize(900, 820); // Используем сохраненный размер
    // startTimerHz(30); // Если таймер нужен для чего-то еще (например, анимации в редакторе)
}

MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor() {
   // processorRef.setSavedEditorSize({ getWidth(), getHeight() }); // Сохраняем размер окна
    processorRef.setCopyToFifo(false);
    setLookAndFeel(nullptr);
}

void MBRPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(ColorScheme::getBackgroundColor());
}

void MBRPAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();

    const int padding = 10;
    const int smallPadding = 5;
    const int controlBarHeight = 32;
    const int analyzerHeight = 300;
    const int labelHeight = 15;
    const int linearSliderHeight = 40;
    const int bandSelectHeight = 30;

    const int rotarySliderDiameter = 100;
    const int rotaryTitleHeight = 18;
    const int rotarySliderTotalHeight = rotaryTitleHeight + rotarySliderDiameter + smallPadding;
    const int rotarySliderWidth = rotarySliderDiameter;

    // 1. Верхняя секция: ControlBar и BypassButton
    auto topSection = bounds.removeFromTop(controlBarHeight).reduced(padding, 0);
    controlBar.setBounds(topSection.removeFromLeft(topSection.getWidth() / 2));
    bypassButton.setBounds(topSection.removeFromRight(60).reduced(0, smallPadding));

    bounds.removeFromTop(padding);

    // 2. Секция анализатора
    auto analyzerArea = bounds.removeFromTop(analyzerHeight).reduced(padding, 0);
    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzerArea);

    bounds.removeFromTop(padding);

    // 3. Секция слайдеров кроссоверов (теперь 3 слайдера)
    int crossoverControlHeight = labelHeight + linearSliderHeight;
    auto crossoverSection = bounds.removeFromTop(crossoverControlHeight).reduced(padding, 0);
    int numCrossoverSliders = 3;
    int singleCrossoverWidth = (crossoverSection.getWidth() - (numCrossoverSliders - 1) * padding) / numCrossoverSliders;

    auto lowMidArea = crossoverSection.removeFromLeft(singleCrossoverWidth);
    lowMidCrossoverLabel.setBounds(lowMidArea.removeFromTop(labelHeight));
    lowMidCrossoverSlider.setBounds(lowMidArea);

    crossoverSection.removeFromLeft(padding);
    auto midArea = crossoverSection.removeFromLeft(singleCrossoverWidth);
    midCrossoverLabel.setBounds(midArea.removeFromTop(labelHeight));
    midCrossoverSlider.setBounds(midArea);

    crossoverSection.removeFromLeft(padding);
    auto midHighArea = crossoverSection; // Оставшееся место
    midHighCrossoverLabel.setBounds(midHighArea.removeFromTop(labelHeight));
    midHighCrossoverSlider.setBounds(midHighArea);

    bounds.removeFromTop(padding);

    // 4. Секция кнопок выбора полосы (BandSelectControls)
    auto bandSelectArea = bounds.removeFromTop(bandSelectHeight);
    // Ширина кнопок выбора полосы, например, 60% ширины редактора
    int bandSelectControlsTargetWidth = juce::jmax(200, getWidth() * 2 / 3); // Минимум 200px, или 2/3 ширины
    bandSelectControls.setBounds(
        bandSelectArea.getCentreX() - bandSelectControlsTargetWidth / 2,
        bandSelectArea.getY(),
        bandSelectControlsTargetWidth,
        bandSelectHeight
    );

    bounds.removeFromTop(padding * 2 + smallPadding);

    // 5. Секция роторных ручек
    auto rotaryMasterArea = bounds.reduced(padding, 0);

    // --- Ручки реверба в ряд ---
    int numReverbKnobs = 4;
    int totalReverbKnobsWidth = numReverbKnobs * rotarySliderWidth + (numReverbKnobs - 1) * padding;
    int reverbKnobsStartX = rotaryMasterArea.getX() + (rotaryMasterArea.getWidth() - totalReverbKnobsWidth) / 2;

    auto reverbKnobsRowArea = rotaryMasterArea.removeFromTop(rotarySliderTotalHeight);
    reverbKnobsRowArea.setX(reverbKnobsStartX);
    reverbKnobsRowArea.setWidth(totalReverbKnobsWidth);

    auto placeRotary = [&](RotarySliderWithLabels& slider, juce::Rectangle<int>& currentPlacementArea) {
        if (currentPlacementArea.getWidth() >= rotarySliderWidth) {
            auto knobArea = currentPlacementArea.removeFromLeft(rotarySliderWidth);
            slider.setBounds(knobArea.getX(),
                knobArea.getY() + (knobArea.getHeight() - rotarySliderTotalHeight) / 2,
                rotarySliderWidth,
                rotarySliderTotalHeight);
            currentPlacementArea.removeFromLeft(padding); // Отступ после ручки
        }
    };

    placeRotary(wetSlider, reverbKnobsRowArea);
    placeRotary(spaceSlider, reverbKnobsRowArea);
    placeRotary(distanceSlider, reverbKnobsRowArea);
    placeRotary(delaySlider, reverbKnobsRowArea);

    rotaryMasterArea.removeFromTop(padding);
    // --- Ручка Pan ПОД ручками реверба и отцентрирована ---
    const int panExternalLabelHeight = 18;
    const int panControlTotalHeight = rotarySliderTotalHeight + smallPadding + (panSlider.getName().isEmpty() ? panExternalLabelHeight : 0);

    if (rotaryMasterArea.getHeight() >= panControlTotalHeight) {
        auto panControlArea = rotaryMasterArea.removeFromTop(panControlTotalHeight);
        int panAreaX = panControlArea.getCentreX() - rotarySliderWidth / 2;
        auto singlePanArea = juce::Rectangle<int>(panAreaX, panControlArea.getY(), rotarySliderWidth, panControlArea.getHeight());

        if (panSlider.getName().isEmpty()) {
            panLabel.setBounds(singlePanArea.removeFromTop(panExternalLabelHeight));
            singlePanArea.removeFromTop(smallPadding);
            panLabel.setVisible(true);
        }
        else {
            panLabel.setVisible(false);
        }
        panSlider.setBounds(singlePanArea.withHeight(rotarySliderTotalHeight));
    }
}

void MBRPAudioProcessorEditor::handleAnalyzerToggle(bool shouldBeOn)
{
    processorRef.setCopyToFifo(shouldBeOn);
    analyzer.setAnalyzerActive(shouldBeOn);
    DBG("Analyzer Toggled: " << (shouldBeOn ? "ON" : "OFF"));
}

void MBRPAudioProcessorEditor::timerCallback() {
    // Если анализатор не активен, нет смысла его обновлять.
    // Однако, анализатор сам имеет таймер, который им управляет.
    // Этот таймер в редакторе может быть не нужен, если все анимации
    // инкапсулированы в компонентах (как в AnalyzerOverlay).
    // Если здесь должна быть логика, связанная с `analyzer.repaintIfActive()`,
    // то `analyzer` должен иметь такой метод, и `analyzer.isAnalyzerActive()` для проверки.
    // В текущей структуре SpectrumAnalyzer из примеров witte/SimpleMBComp, он сам себя обновляет.
}

void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex) { // bandIndex 0..3
    juce::String paramId;
    juce::Colour bandColour;

    switch (bandIndex) {
    case 0: // Low
        paramId = "lowPan";
        bandColour = ColorScheme::getLowBandColor();
        break;
    case 1: // Low-Mid
        paramId = "lowMidPan";
        // Можно определить новый цвет для Low-Mid, например, смесь оранжевого и зеленого, или новый уникальный
        bandColour = ColorScheme::getLowMidBandColor(); // Предполагаем, что этот цвет добавлен в ColorScheme
        break;
    case 2: // Mid-High
        paramId = "midHighPan";
        bandColour = ColorScheme::getMidHighBandColor(); // Этот цвет уже был (бывший Mid)
        break;
    case 3: // High
        paramId = "highPan";
        bandColour = ColorScheme::getHighBandAltColor(); // Этот цвет уже был (бывший High), возможно переименовать
        break;
    default: jassertfalse; return;
    }

    auto* targetParam = processorRef.getAPVTS().getParameter(paramId);
    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
    jassert(rangedParam != nullptr); if (!rangedParam) return;

    panSlider.changeParam(rangedParam);

    panSlider.labels.clear();
    panSlider.labels.add({ 0.0f, "L" });
    panSlider.labels.add({ 0.5f, "C" });
    panSlider.labels.add({ 1.0f, "R" });

    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    panAttachment.reset();
    panAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramId, panSlider);

    panLabel.setText("Pan", juce::dontSendNotification);

    panSlider.setColour(juce::Slider::thumbColourId, bandColour);
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, bandColour.withAlpha(0.7f));
    panSlider.setColour(juce::Slider::rotarySliderOutlineColourId, bandColour.darker(0.3f));
    panSlider.repaint();
}

void MBRPAudioProcessorEditor::handleBandAreaClick(int bandIndex) // bandIndex 0..3
{
    DBG("Editor received band click from overlay: " << bandIndex);

    juce::ToggleButton* buttonToSelect = nullptr;
    switch (bandIndex) {
    case 0: buttonToSelect = &bandSelectControls.lowBandButton; break;
    case 1: buttonToSelect = &bandSelectControls.lowMidBandButton; break; // Новая кнопка
    case 2: buttonToSelect = &bandSelectControls.midHighBandButton; break;// Старая midBandButton -> midHighBandButton
    case 3: buttonToSelect = &bandSelectControls.highBandButton; break;  // Старая highBandButton
    default:
        jassertfalse;
        return;
    }

    if (buttonToSelect && !buttonToSelect->getToggleState())
    {
        buttonToSelect->setToggleState(true, juce::sendNotificationSync);
    }
}

static juce::String rangedParamToString(double value, int decimalPlaces = 0)
{
    // Если значение очень близко к целому, округляем
    if (std::abs(value - std::round(value)) < 0.0001 && decimalPlaces == 0) {
        return juce::String(static_cast<int>(std::round(value)));
    }
    return juce::String(value, decimalPlaces);
}

void MBRPAudioProcessorEditor::updateReverbAttachments(int bandIndex) { // bandIndex 0..3
    juce::String wetParamId, spaceParamId, distanceParamId, delayParamId;
    juce::Colour bandColour;

    switch (bandIndex) {
    case 0: // Low
        wetParamId = "lowWet"; spaceParamId = "lowSpace"; distanceParamId = "lowDistance"; delayParamId = "lowDelay";
        bandColour = ColorScheme::getLowBandColor();
        break;
    case 1: // Low-Mid
        wetParamId = "lowMidWet"; spaceParamId = "lowMidSpace"; distanceParamId = "lowMidDistance"; delayParamId = "lowMidDelay";
        bandColour = ColorScheme::getLowMidBandColor();
        break;
    case 2: // Mid-High
        wetParamId = "midHighWet"; spaceParamId = "midHighSpace"; distanceParamId = "midHighDistance"; delayParamId = "midHighDelay";
        bandColour = ColorScheme::getMidHighBandColor();
        break;
    case 3: // High
        wetParamId = "highWet"; spaceParamId = "highSpace"; distanceParamId = "highDistance"; delayParamId = "highDelay";
        bandColour = ColorScheme::getHighBandAltColor();
        break;
    default:
        jassertfalse;
        return;
    }

    auto updateSingleReverbSlider =
        [&](RotarySliderWithLabels& slider, const juce::String& paramID,
            std::unique_ptr<SliderAttachment>& attachment,
            const juce::String& minLabel, const juce::String& maxLabelOrGetter)
    {
        auto* targetParam = processorRef.getAPVTS().getParameter(paramID);
        auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
        jassert(rangedParam != nullptr); if (!rangedParam) return;

        slider.changeParam(rangedParam); // Обновляем параметр в слайдере
        attachment.reset();
        attachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramID, slider);

        slider.labels.clear();
        if (!minLabel.isEmpty()) slider.labels.add({ 0.0f, minLabel });

        juce::String actualMaxLabel = maxLabelOrGetter;
        // Если maxLabelOrGetter это специальная строка, то получаем значение из параметра
        if (maxLabelOrGetter == "GET_MAX_FROM_PARAM_MS") {
            actualMaxLabel = rangedParamToString(rangedParam->getNormalisableRange().end) + "ms";
        }
        else if (maxLabelOrGetter == "GET_MAX_FROM_PARAM_PERCENT") {
            actualMaxLabel = rangedParamToString(rangedParam->getNormalisableRange().end * 100.f) + "%";
        }


        if (!actualMaxLabel.isEmpty()) slider.labels.add({ 1.0f, actualMaxLabel });

        slider.setColour(juce::Slider::thumbColourId, bandColour);
        slider.setColour(juce::Slider::rotarySliderFillColourId, bandColour.withAlpha(0.7f));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, bandColour.darker(0.3f));
        slider.repaint();
    };

    // Для Pre-Delay используем специальную метку, чтобы получить максимальное значение из параметра
    updateSingleReverbSlider(wetSlider, wetParamId, wetAttachment, "0%", "100%");
    updateSingleReverbSlider(spaceSlider, spaceParamId, spaceAttachment, "0%", "100%");
    updateSingleReverbSlider(distanceSlider, distanceParamId, distanceAttachment, "0%", "100%");
    updateSingleReverbSlider(delaySlider, delayParamId, delayAttachment, "0ms", "GET_MAX_FROM_PARAM_MS");
}
