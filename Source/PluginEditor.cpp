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
    const int titleBarHeight = 30; // Опциональная высота для заголовка "MBRP" и кнопки Bypass

    // --- 1. Верхняя панель (Название плагина "MBRP" и общий Bypass) ---
    auto titleArea = bounds.removeFromTop(titleBarHeight).reduced(padding, 0);
    // bypassButton размещаем справа на этой панели
    bypassButton.setBounds(titleArea.removeFromRight(60).reduced(0, smallPadding / 2));
    // Оставшееся место в titleArea можно использовать для текста "MBRP" (см. paint)

    bounds.removeFromTop(smallPadding); // Отступ после верхней панели

    // --- 2. Секция слайдеров кроссоверов ---
    const int labelHeight = 15;
    const int linearSliderHeight = 40;
    const int crossoverControlHeight = labelHeight + linearSliderHeight;
    auto crossoverSection = bounds.removeFromTop(crossoverControlHeight).reduced(padding, 0);
    // ... (логика размещения 3-х слайдеров кроссовера без изменений) ...
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
    auto midHighArea = crossoverSection;
    midHighCrossoverLabel.setBounds(midHighArea.removeFromTop(labelHeight));
    midHighCrossoverSlider.setBounds(midHighArea);
    bounds.removeFromTop(padding);

    // --- 3. Панель выбора активной полосы и кнопки S/M/B под ней ---
    const int bandSelectHeight = 30;
    const int smbButtonsHeight = 25; // Высота для кнопок S/M/B
    auto bandControlsArea = bounds.removeFromTop(bandSelectHeight + smbButtonsHeight + smallPadding).reduced(padding, 0);

    auto bandSelectRect = bandControlsArea.removeFromTop(bandSelectHeight);
    int bandSelectControlsTargetWidth = juce::jmax(240, getWidth() * 3 / 5);
    bandSelectControls.setBounds(
        bandSelectRect.getCentreX() - bandSelectControlsTargetWidth / 2,
        bandSelectRect.getY(),
        bandSelectControlsTargetWidth,
        bandSelectHeight
    );

    bandControlsArea.removeFromTop(smallPadding);
    auto smbRect = bandControlsArea;
    juce::FlexBox smbFlexBox;
    smbFlexBox.flexDirection = juce::FlexBox::Direction::row;
    smbFlexBox.justifyContent = juce::FlexBox::JustifyContent::center;
    smbFlexBox.alignItems = juce::FlexBox::AlignItems::center;
    const int smbButtonWidth = 40;
    smbFlexBox.items.add(juce::FlexItem(bandBypassButton).withWidth(smbButtonWidth).withHeight(smbButtonsHeight).withMargin(smallPadding));
    smbFlexBox.items.add(juce::FlexItem(bandSoloButton).withWidth(smbButtonWidth).withHeight(smbButtonsHeight).withMargin(smallPadding));
    smbFlexBox.items.add(juce::FlexItem(bandMuteButton).withWidth(smbButtonWidth).withHeight(smbButtonsHeight).withMargin(smallPadding));
    int totalSmbWidth = 3 * smbButtonWidth + 4 * smallPadding;
    smbFlexBox.performLayout(smbRect.withSizeKeepingCentre(totalSmbWidth, smbButtonsHeight));
    bounds.removeFromTop(padding);

    // --- 4. Ряд радиальных слайдеров (Gain, Reverb, Pan) ---
    const int rotarySliderDiameter = 80;
    const int rotaryTitleHeight = 16;    // Уменьшил немного для компактности
    const int rotarySliderTotalHeight = rotaryTitleHeight + rotarySliderDiameter + smallPadding;

    auto mainControlsRowArea = bounds.removeFromTop(rotarySliderTotalHeight).reduced(padding, 0);

    juce::FlexBox mainControlsFlexBox;
    mainControlsFlexBox.flexDirection = juce::FlexBox::Direction::row;
    mainControlsFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    mainControlsFlexBox.alignItems = juce::FlexBox::AlignItems::center;

    RotarySliderWithLabels* slidersInRow[] = { &gainSlider, &wetSlider, &spaceSlider, &distanceSlider, &delaySlider, &panSlider };
    juce::Label* labelsForSliders[] = { nullptr, nullptr, nullptr, nullptr, nullptr, &panLabel }; // panLabel для panSlider

    for (int i = 0; i < 6; ++i) {
        auto& slider = *slidersInRow[i];
        auto* label = labelsForSliders[i];

        if (label && slider.getName().isEmpty()) { // Если у слайдера нет своего Title, используем внешнюю метку над ним
            auto container = std::make_unique<juce::Component>(); // Используем unique_ptr для управления
            addAndMakeVisible(*container);
            slider.setBounds(0, rotaryTitleHeight + smallPadding / 2, rotarySliderDiameter, rotarySliderDiameter);
            label->setBounds(0, 0, rotarySliderDiameter, rotaryTitleHeight);
            label->setJustificationType(juce::Justification::centredBottom); // Выравниваем метку
            container->addAndMakeVisible(slider);
            container->addAndMakeVisible(*label);
            mainControlsFlexBox.items.add(juce::FlexItem(*container).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
            // ВАЖНО: нужно где-то хранить эти unique_ptr<Component> контейнеры, чтобы они не удалялись.
            // Либо не использовать unique_ptr, а добавлять их как члены класса PluginEditor.
            // Для простоты пока без этого сложного контейнера, panLabel будет просто позиционироваться.
            // Вместо этого сделаем проще:
            // mainControlsFlexBox.items.add(juce::FlexItem(slider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
            // panLabel.setBounds(panSlider.getX(), panSlider.getY() - rotaryTitleHeight - smallPadding, rotarySliderDiameter, rotaryTitleHeight);

        }
        else {
            mainControlsFlexBox.items.add(juce::FlexItem(slider).withWidth(rotarySliderDiameter).withHeight(rotarySliderTotalHeight));
            if (label) label->setVisible(false); // Скрываем внешнюю метку, если у слайдера есть своя
        }
    }
    mainControlsFlexBox.performLayout(mainControlsRowArea);

    // Корректировка позиции panLabel, если он видим
    if (panLabel.isVisible()) {
        panLabel.setBounds(panSlider.getX(), mainControlsRowArea.getY(), rotarySliderDiameter, rotaryTitleHeight);
    }


    bounds.removeFromTop(padding);

    // --- 5. Область анализатора и кнопка его включения/выключения ---
    // Кнопка теперь будет справа сверху от этой финальной области 'bounds'
    const int analyzerButtonSize = 28;
    auto analyzerArea = bounds.reduced(padding, padding); // Основная область для графика

    controlBar.analyzerButton.setBounds(analyzerArea.getRight() - analyzerButtonSize - smallPadding,
        analyzerArea.getY() + smallPadding,
        analyzerButtonSize, analyzerButtonSize);
    controlBar.analyzerButton.toFront(true); // Убедимся, что кнопка поверх анализатора

    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzer.getBounds());
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
