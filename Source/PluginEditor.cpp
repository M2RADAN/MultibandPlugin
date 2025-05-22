#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- ControlBar Implementation ---
// ControlBar теперь просто логический владелец кнопки и ее колбэка,
// сам он не будет добавляться как видимый компонент в PluginEditor.
ControlBar::ControlBar() {
    analyzerButton.setClickingTogglesState(true);
    analyzerButton.setTooltip("Enable/Disable Spectrum Analyzer");
    analyzerButton.onClick = [this] { analyzerButtonToggled(); };
    // addAndMakeVisible(analyzerButton); // Не делаем здесь, т.к. кнопка будет добавлена в Editor
}
void ControlBar::resized() {
    // Этот resized для ControlBar больше не актуален, если ControlBar не видима
    // analyzerButton.setBounds(getLocalBounds()); 
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
    analyzerOverlay(p), // Передаем весь процессор
    wetSlider(nullptr, " %", "Wet"),
    spaceSlider(nullptr, " %", "Space"),
    distanceSlider(nullptr, " %", "Distance"),
    delaySlider(nullptr, " ms", "Pre-Delay"),
    panSlider(nullptr, "", "Pan"),
    gainSlider(nullptr, " dB", "Gain"),
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midCrossoverAttachment(processorRef.getAPVTS(), "midCrossover", midCrossoverSlider),
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider)
{
    setLookAndFeel(&lnf);
    processorRef.setCopyToFifo(true);

    // Добавляем компоненты
    addAndMakeVisible(controlBar.analyzerButton); // Добавляем кнопку напрямую
    addAndMakeVisible(bypassButton);

    addAndMakeVisible(lowMidCrossoverSlider); addAndMakeVisible(lowMidCrossoverLabel);
    addAndMakeVisible(midCrossoverSlider);    addAndMakeVisible(midCrossoverLabel);
    addAndMakeVisible(midHighCrossoverSlider); addAndMakeVisible(midHighCrossoverLabel);

    addAndMakeVisible(bandSelectControls);

    addAndMakeVisible(bandBypassButton);
    addAndMakeVisible(bandSoloButton);
    addAndMakeVisible(bandMuteButton);

    addAndMakeVisible(gainSlider);
    addAndMakeVisible(wetSlider);
    addAndMakeVisible(spaceSlider);
    addAndMakeVisible(distanceSlider);
    addAndMakeVisible(delaySlider);
    addAndMakeVisible(panSlider);
    if (panSlider.getName().isEmpty()) // Делаем panLabel видимой только если у panSlider нет своего title
        addAndMakeVisible(panLabel);

    addAndMakeVisible(analyzer);
    addAndMakeVisible(analyzerOverlay);

    auto setupRotarySliderComponent = [&](RotarySliderWithLabels& slider) {
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled(true, true, this, 500);
        };

    setupRotarySliderComponent(wetSlider); /* ... и для всех остальных роторных ... */
    setupRotarySliderComponent(spaceSlider);
    setupRotarySliderComponent(distanceSlider);
    setupRotarySliderComponent(delaySlider);
    setupRotarySliderComponent(gainSlider);
    setupRotarySliderComponent(panSlider);

    controlBar.onAnalyzerToggle = [this](bool state) { handleAnalyzerToggle(state); };
    bool initialAnalyzerState = processorRef.isCopyToFifoEnabled();
    controlBar.analyzerButton.setToggleState(initialAnalyzerState, juce::dontSendNotification);
    handleAnalyzerToggle(initialAnalyzerState);

    globalBypassAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "bypass", bypassButton);
    // bypassButton.setButtonText("Bypass"); // PowerButton обычно не показывает текст
    bypassButton.setTooltip("Bypass the plugin processing");
    bypassButton.setClickingTogglesState(true);

    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20);
        slider.setPopupDisplayEnabled(true, false, this);
        slider.setColour(juce::Slider::thumbColourId, ColorScheme::getSliderThumbColor());
        slider.setColour(juce::Slider::trackColourId, ColorScheme::getSliderTrackColor());
        slider.setColour(juce::Slider::textBoxTextColourId, ColorScheme::getSecondaryTextColor());
        slider.setColour(juce::Slider::textBoxOutlineColourId, ColorScheme::getSliderTrackColor().darker(0.2f));
        slider.setName(labelText);
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f));
        label.setColour(juce::Label::textColourId, ColorScheme::getTextColor());
        };
    setupStandardSlider(lowMidCrossoverSlider, lowMidCrossoverLabel, "Low / L-Mid");
    setupStandardSlider(midCrossoverSlider, midCrossoverLabel, "L-Mid / M-High");
    setupStandardSlider(midHighCrossoverSlider, midHighCrossoverLabel, "M-High / High");

    panLabel.setText("Pan", juce::dontSendNotification);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    panLabel.setColour(juce::Label::textColourId, ColorScheme::getDarkTextColor());
    // addAndMakeVisible(panLabel); // Уже сделано

    auto setupBandToggleButton = [&](juce::TextButton& button, const juce::String& text) {
        button.setButtonText(text);
        button.setClickingTogglesState(true);
        // addAndMakeVisible(button); // Уже сделано
        };
    setupBandToggleButton(bandBypassButton, "B");
    setupBandToggleButton(bandSoloButton, "S");
    setupBandToggleButton(bandMuteButton, "M");
    bandBypassButton.setTooltip("Bypass selected band effects");
    bandSoloButton.setTooltip("Solo selected band");
    bandMuteButton.setTooltip("Mute selected band");

    bandSelectControls.onBandSelected = [this](int bandIndex) {
        currentSelectedBand = bandIndex;
        updatePanAttachment(bandIndex);
        analyzerOverlay.setActiveBand(bandIndex);
        updateReverbAttachments(bandIndex);
        updateBandSpecificControls(bandIndex);
        };
    analyzerOverlay.onBandAreaClicked = [this](int bandIndex) { handleBandAreaClick(bandIndex); };

    currentSelectedBand = 0;
    updatePanAttachment(currentSelectedBand);
    updateReverbAttachments(currentSelectedBand);
    updateBandSpecificControls(currentSelectedBand);
    analyzerOverlay.setActiveBand(currentSelectedBand);
    // Увеличим высоту по умолчанию, чтобы вместить все контролы сверху
    setSize(900, 600); // Примерная высота, подберите по факту
}

MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor() {
    // processorRef.setSavedEditorSize({ getWidth(), getHeight() }); // Раскомментируйте, если хотите сохранять размер
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
    const int titleBarHeight = 30;

    // --- 1. Верхняя панель (Название плагина "MBRP" и общий Bypass) ---
    auto titleArea = bounds.removeFromTop(titleBarHeight).reduced(padding, 0);
    bypassButton.setBounds(titleArea.removeFromRight(60).reduced(0, smallPadding / 2));
    bounds.removeFromTop(smallPadding);

    // --- Зона для контролов, которые располагаются над анализатором, когда он ВИДЕН ---
    // (Gain, Reverb, Pan)
    const int rotarySliderDiameter = 80;
    const int rotaryTitleHeight = 16;
    const int rotarySliderTotalHeight = rotaryTitleHeight + rotarySliderDiameter + smallPadding;
    auto topControlsArea = bounds.removeFromTop(rotarySliderTotalHeight).reduced(padding, 0);

    juce::FlexBox mainControlsFlexBox;
    mainControlsFlexBox.flexDirection = juce::FlexBox::Direction::row;
    mainControlsFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    mainControlsFlexBox.alignItems = juce::FlexBox::AlignItems::center;

    // Добавляем слайдеры в FlexBox (Gain, Reverb, Pan)
    mainControlsFlexBox.items.add(juce::FlexItem(gainSlider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
    mainControlsFlexBox.items.add(juce::FlexItem(wetSlider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
    mainControlsFlexBox.items.add(juce::FlexItem(spaceSlider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
    mainControlsFlexBox.items.add(juce::FlexItem(distanceSlider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
    mainControlsFlexBox.items.add(juce::FlexItem(delaySlider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));

    if (panSlider.getName().isEmpty()) {
        // Если panSlider не имеет своего title, значит panLabel должна быть видима
        // и нужно создать контейнер для panSlider и panLabel.
        // Для простоты, здесь мы просто разместим panSlider, а panLabel будет позиционироваться относительно него.
        mainControlsFlexBox.items.add(juce::FlexItem(panSlider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
        panLabel.setVisible(true);
    }
    else {
        mainControlsFlexBox.items.add(juce::FlexItem(panSlider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
        panLabel.setVisible(false);
    }
    mainControlsFlexBox.performLayout(topControlsArea);

    // Корректировка позиции panLabel
    if (panLabel.isVisible()) {
        panLabel.setBounds(panSlider.getX(), topControlsArea.getY() - rotaryTitleHeight - smallPadding, rotarySliderDiameter, rotaryTitleHeight);
        // Может потребоваться более точное позиционирование, если FlexBox сдвинул слайдеры
        // или просто panLabel.setTopLeftPosition(panSlider.getX(), panSlider.getY() - rotaryTitleHeight - smallPadding);
    }
    bounds.removeFromTop(padding);

    // --- Область, где будет либо анализатор, либо альтернативные контролы ---
    auto mainDisplayArea = bounds.reduced(padding, padding);

    // Кнопка включения/выключения анализатора всегда видна, справа сверху от этой области
    const int analyzerButtonSize = 28;
    controlBar.analyzerButton.setBounds(mainDisplayArea.getRight() - analyzerButtonSize - smallPadding,
        mainDisplayArea.getY() + smallPadding,
        analyzerButtonSize, analyzerButtonSize);
    controlBar.analyzerButton.toFront(true);

    if (analyzer.isVisible()) // Если анализатор должен быть виден
    {
        analyzer.setBounds(mainDisplayArea);
        analyzerOverlay.setBounds(analyzer.getBounds());
    }
    else // Иначе, на этом месте показываем контролы кроссоверов и выбора полосы
    {
        auto alternativeControlsArea = mainDisplayArea; // Используем ту же область

        // 1. Секция слайдеров кроссоверов
        const int labelHeightAlt = 15;
        const int linearSliderHeightAlt = 40;
        const int crossoverControlHeightAlt = labelHeightAlt + linearSliderHeightAlt;
        auto crossoverSectionAlt = alternativeControlsArea.removeFromTop(crossoverControlHeightAlt);

        int numCrossoverSlidersAlt = 3;
        int singleCrossoverWidthAlt = (crossoverSectionAlt.getWidth() - (numCrossoverSlidersAlt - 1) * padding) / numCrossoverSlidersAlt;

        auto lowMidAreaAlt = crossoverSectionAlt.removeFromLeft(singleCrossoverWidthAlt);
        lowMidCrossoverLabel.setBounds(lowMidAreaAlt.removeFromTop(labelHeightAlt));
        lowMidCrossoverSlider.setBounds(lowMidAreaAlt);
        crossoverSectionAlt.removeFromLeft(padding);
        auto midAreaAlt = crossoverSectionAlt.removeFromLeft(singleCrossoverWidthAlt);
        midCrossoverLabel.setBounds(midAreaAlt.removeFromTop(labelHeightAlt));
        midCrossoverSlider.setBounds(midAreaAlt);
        crossoverSectionAlt.removeFromLeft(padding);
        auto midHighAreaAlt = crossoverSectionAlt;
        midHighCrossoverLabel.setBounds(midHighAreaAlt.removeFromTop(labelHeightAlt));
        midHighCrossoverSlider.setBounds(midHighAreaAlt);

        alternativeControlsArea.removeFromTop(padding);

        // 2. Панель выбора активной полосы и кнопки S/M/B под ней
        const int bandSelectHeightAlt = 30;
        const int smbButtonsHeightAlt = 25;
        auto bandSelectAreaAlt = alternativeControlsArea.removeFromTop(bandSelectHeightAlt);
        int bandSelectControlsTargetWidthAlt = juce::jmax(240, alternativeControlsArea.getWidth() * 4 / 5);
        bandSelectControls.setBounds(
            bandSelectAreaAlt.getCentreX() - bandSelectControlsTargetWidthAlt / 2,
            bandSelectAreaAlt.getY(),
            bandSelectControlsTargetWidthAlt,
            bandSelectHeightAlt
        );

        alternativeControlsArea.removeFromTop(smallPadding);
        auto smbRectAlt = alternativeControlsArea.removeFromTop(smbButtonsHeightAlt);
        juce::FlexBox smbFlexBoxAlt;
        smbFlexBoxAlt.flexDirection = juce::FlexBox::Direction::row;
        smbFlexBoxAlt.justifyContent = juce::FlexBox::JustifyContent::center;
        smbFlexBoxAlt.alignItems = juce::FlexBox::AlignItems::center;
        const int smbButtonWidthAlt = 40;
        smbFlexBoxAlt.items.add(juce::FlexItem(bandBypassButton).withWidth(smbButtonWidthAlt).withHeight(smbButtonsHeightAlt).withMargin(smallPadding));
        smbFlexBoxAlt.items.add(juce::FlexItem(bandSoloButton).withWidth(smbButtonWidthAlt).withHeight(smbButtonsHeightAlt).withMargin(smallPadding));
        smbFlexBoxAlt.items.add(juce::FlexItem(bandMuteButton).withWidth(smbButtonWidthAlt).withHeight(smbButtonsHeightAlt).withMargin(smallPadding));
        int totalSmbWidthAlt = 3 * smbButtonWidthAlt + 4 * smallPadding;
        smbFlexBoxAlt.performLayout(smbRectAlt.withSizeKeepingCentre(totalSmbWidthAlt, smbButtonsHeightAlt));
    }
}


void MBRPAudioProcessorEditor::handleAnalyzerToggle(bool shouldBeOn)
{
    processorRef.setCopyToFifo(shouldBeOn);
    analyzer.setAnalyzerActive(shouldBeOn); // Этот метод в SpectrumAnalyzer должен управлять его внутренней логикой и перерисовкой

    // Управляем видимостью компонентов
    analyzer.setVisible(shouldBeOn);
    analyzerOverlay.setVisible(shouldBeOn);

    // Контролы кроссоверов и выбора полосы показываются, когда анализатор ВЫКЛЮЧЕН
    bool showAlternativeControls = !shouldBeOn;
    lowMidCrossoverSlider.setVisible(showAlternativeControls);
    lowMidCrossoverLabel.setVisible(showAlternativeControls);
    midCrossoverSlider.setVisible(showAlternativeControls);
    midCrossoverLabel.setVisible(showAlternativeControls);
    midHighCrossoverSlider.setVisible(showAlternativeControls);
    midHighCrossoverLabel.setVisible(showAlternativeControls);
    bandSelectControls.setVisible(showAlternativeControls);
    // Кнопки S/M/B также зависят от этого
    bandBypassButton.setVisible(showAlternativeControls);
    bandSoloButton.setVisible(showAlternativeControls);
    bandMuteButton.setVisible(showAlternativeControls);


    DBG("Analyzer Toggled: " << (shouldBeOn ? "ON" : "OFF") << ", AltControls: " << (showAlternativeControls ? "ON" : "OFF"));

    // Пересчитываем компоновку, так как видимость ключевых блоков изменилась
    resized();
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

void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex) {
    juce::String paramId;
    juce::Colour bandColour;
    switch (bandIndex) {
    case 0: paramId = "lowPan"; bandColour = ColorScheme::getLowBandColor(); break;
    case 1: paramId = "lowMidPan"; bandColour = ColorScheme::getLowMidBandColor(); break;
    case 2: paramId = "midHighPan"; bandColour = ColorScheme::getMidHighBandColor(); break;
    case 3: paramId = "highPan"; bandColour = ColorScheme::getHighBandAltColor(); break;
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
    panLabel.setText("Pan", juce::dontSendNotification); // Внешняя метка остается "Pan"
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
    case 1: buttonToSelect = &bandSelectControls.lowMidBandButton; break;
    case 2: buttonToSelect = &bandSelectControls.midHighBandButton; break;
    case 3: buttonToSelect = &bandSelectControls.highBandButton; break;
    default: jassertfalse; return;
    }
    if (buttonToSelect && !buttonToSelect->getToggleState())
    {
        buttonToSelect->setToggleState(true, juce::sendNotificationSync);
    }
    else if (buttonToSelect && buttonToSelect->getToggleState()) {
        // Если кликнули на уже активную кнопку (например, через маркер Gain),
        // все равно нужно убедиться, что оверлей знает об этом.
        // Хотя onBandSelected не вызовется второй раз, если состояние не изменилось.
        // Поэтому, если это был клик по маркеру Gain, analyzerOverlay сам обновит активную полосу.
        // Если это был клик по области, и кнопка уже активна, то можно и здесь вызвать:
        analyzerOverlay.setActiveBand(bandIndex); // На всякий случай, если onBandSelected не сработал
    }
}

static juce::String rangedParamToString(double value, int decimalPlaces = 0)
{
    if (std::abs(value - std::round(value)) < 0.001 && decimalPlaces == 0) {
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

// Обновление контролов громкости, S/M/B для выбранной полосы ---
void MBRPAudioProcessorEditor::updateBandSpecificControls(int bandIndex)
{
    juce::String gainParamID, bypassParamID, soloParamID, muteParamID;
    juce::Colour bandColour;

    switch (bandIndex)
    {
    case 0: // Low
        gainParamID = "lowGain"; bypassParamID = "lowBypass"; soloParamID = "lowSolo"; muteParamID = "lowMute";
        bandColour = ColorScheme::getLowBandColor();
        break;
    case 1: // Low-Mid
        gainParamID = "lowMidGain"; bypassParamID = "lowMidBypass"; soloParamID = "lowMidSolo"; muteParamID = "lowMidMute";
        bandColour = ColorScheme::getLowMidBandColor();
        break;
    case 2: // Mid-High
        gainParamID = "midHighGain"; bypassParamID = "midHighBypass"; soloParamID = "midHighSolo"; muteParamID = "midHighMute";
        bandColour = ColorScheme::getMidHighBandColor();
        break;
    case 3: // High
        gainParamID = "highGain"; bypassParamID = "highBypass"; soloParamID = "highSolo"; muteParamID = "highMute";
        bandColour = ColorScheme::getHighBandAltColor();
        break;
    default:
        jassertfalse; return;
    }

    // Обновление слайдера громкости
    auto* gainRAP = dynamic_cast<juce::RangedAudioParameter*>(processorRef.getAPVTS().getParameter(gainParamID));
    jassert(gainRAP != nullptr);
    if (gainRAP) {
        gainSlider.changeParam(gainRAP);
        gainAttachment.reset();
        gainAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), gainParamID, gainSlider);
        gainSlider.setColour(juce::Slider::thumbColourId, bandColour);
        gainSlider.setColour(juce::Slider::rotarySliderFillColourId, bandColour.withAlpha(0.7f));
        gainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, bandColour.darker(0.3f));
        gainSlider.labels.clear(); // Очищаем метки L/C/R, если они были от Pan
        // Можно добавить метки Min/Max для громкости, если нужно
        gainSlider.labels.add({ 0.0f, rangedParamToString(gainSlider.getRange().getStart(), 0) + "dB" });
        gainSlider.labels.add({ 1.0f, rangedParamToString(gainSlider.getRange().getEnd(), 0) + "dB" });
        gainSlider.repaint();
    }

    // Обновление аттачментов для кнопок S/M/B
    bandBypassAttachment.reset();
    bandBypassAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), bypassParamID, bandBypassButton);

    bandSoloAttachment.reset();
    bandSoloAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), soloParamID, bandSoloButton);

    bandMuteAttachment.reset();
    bandMuteAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), muteParamID, bandMuteButton);

    // Обновление внешнего вида кнопок (если LookAndFeel не делает это автоматически при смене аттачмента)
    // Для TextButton, LookAndFeel должен сам обновлять их при изменении toggleState,
    // но цвет может зависеть от bandColour. Это можно добавить в LookAndFeel::drawToggleButton.
    // bandBypassButton.setColour(juce::TextButton::buttonOnColourId, bandColour.darker()); 
    // ... и т.д. или просто repaint()
    bandBypassButton.repaint();
    bandSoloButton.repaint();
    bandMuteButton.repaint();
}
