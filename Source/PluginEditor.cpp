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
    wetSlider(nullptr, " %", "Wet"),          // param, suffix, title
    spaceSlider(nullptr, " %", "Space"),
    distanceSlider(nullptr, " %", "Distance"),
    delaySlider(nullptr, " ms", "Pre-Delay"),
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

    // --- НОВОЕ: Настройка слайдеров реверба ---
    auto setupReverbSliderComponent = [&](RotarySliderWithLabels& slider) {
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // <<-- УБИРАЕМ ТЕКСТОВОЕ ПОЛЕ
        slider.setPopupDisplayEnabled(true, false, this);
        // Цвета для самого слайдера (ручка, заливка) будут установлены в updateReverbAttachments
        // и LookAndFeel::drawRotarySlider
        addAndMakeVisible(slider);
        };

    setupReverbSliderComponent(wetSlider);
    setupReverbSliderComponent(spaceSlider);
    setupReverbSliderComponent(distanceSlider);
    setupReverbSliderComponent(delaySlider);
    // ----------------------------------------

    controlBar.onAnalyzerToggle = [this](bool state) { handleAnalyzerToggle(state); };
    bool initialAnalyzerState = processorRef.isCopyToFifoEnabled(); // Проверяем начальное состояние процессора
    controlBar.analyzerButton.setToggleState(initialAnalyzerState, juce::dontSendNotification);
    handleAnalyzerToggle(initialAnalyzerState); // Применяем начальное состояние

    bypassAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "bypass", bypassButton);

    bypassButton.setButtonText("Bypass"); // Текст может не отображаться для PowerButton
    bypassButton.setTooltip("Bypass the plugin processing");
    bypassButton.setClickingTogglesState(true); // Обязательно для аттачмента
    addAndMakeVisible(bypassButton);


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
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // <<-- УБИРАЕМ ТЕКСТОВОЕ ПОЛЕ
    panSlider.setPopupDisplayEnabled(true, false, this);
    // Цвета будут установлены в updatePanAttachment
    addAndMakeVisible(panSlider);

    // Внешняя метка "Pan"
    panLabel.setText("Pan", juce::dontSendNotification);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setFont(juce::Font(14.0f, juce::Font::bold)); // Шрифт как у заголовков RotarySliderWithLabels
    panLabel.setColour(juce::Label::textColourId, ColorScheme::getDarkTextColor());
    addAndMakeVisible(panLabel);

    bandSelectControls.onBandSelected = [this](int bandIndex) {
        updatePanAttachment(bandIndex);
        updateReverbAttachments(bandIndex); // <<< ВЫЗЫВАЕМ ОБНОВЛЕНИЕ РЕВЕРБА
    };
    analyzerOverlay.onBandAreaClicked = [this](int bandIndex) { handleBandAreaClick(bandIndex); };

    // --- Инициализация аттачментов для выбранной полосы (0 - Low) ---
    updatePanAttachment(0);
    updateReverbAttachments(0); // <<< ИНИЦИАЛИЗИРУЕМ РЕВЕРБ

    setSize(900, 820);
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
    auto bounds = getLocalBounds(); // Получаем текущие границы всего редактора

    // --- Константы для отступов и размеров элементов ---
    const int padding = 10;         // Общий отступ
    const int smallPadding = 5;     // Маленький отступ для внутренних элементов
    const int controlBarHeight = 32;
    const int analyzerHeight = 300;
    const int labelHeight = 15;         // Высота метки для линейного слайдера
    const int linearSliderHeight = 40;  // Высота самого линейного слайдера
    const int bandSelectHeight = 30;    // Высота для компонента BandSelectControls

    // Размеры для роторных ручек
    const int rotaryLabelHeight = 20;   // Высота для метки НАД роторной ручкой
    const int rotaryKnobWidth = 80;     // Желаемая ширина области для ручки
/*    const int rotarySliderWidth = 80;*/  // Общая ширина для одного RotarySliderWithLabels
    const int rotarySliderHeight = 100; // Общая высота (включая title и textbox)
    const int rotaryKnobVisualSize = 80; // Фактический диаметр видимой части ручки (чуть меньше области)
    const int rotaryKnobHeightWithText = rotaryKnobVisualSize + smallPadding + rotaryLabelHeight; // Высота ручки + отступ + высота текстбокса

    const int rotarySliderInternalTitleHeight = wetSlider.getTextHeight() + 2; // Примерная высота заголовка внутри
    const int rotarySliderCircleDiameter = 70; // Желаемый диаметр круга
    const int rotarySliderTotalHeight = rotarySliderInternalTitleHeight + rotarySliderCircleDiameter + smallPadding; // Общая высота для компонента
    const int rotarySliderWidth = rotarySliderCircleDiameter; // Ширина равна диаметру круга

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

    // 3. Секция слайдеров кроссоверов
    int crossoverControlHeight = labelHeight + linearSliderHeight;
    auto crossoverSection = bounds.removeFromTop(crossoverControlHeight).reduced(padding, 0);
    int singleCrossoverWidth = crossoverSection.getWidth() / 2 - padding / 2;
    auto lowMidArea = crossoverSection.removeFromLeft(singleCrossoverWidth);
    lowMidCrossoverLabel.setBounds(lowMidArea.removeFromTop(labelHeight));
    lowMidCrossoverSlider.setBounds(lowMidArea);
    crossoverSection.removeFromLeft(padding);
    auto midHighArea = crossoverSection;
    midHighCrossoverLabel.setBounds(midHighArea.removeFromTop(labelHeight));
    midHighCrossoverSlider.setBounds(midHighArea);

    bounds.removeFromTop(padding);

    // 4. Секция кнопок выбора полосы (BandSelectControls)
    auto bandSelectArea = bounds.removeFromTop(bandSelectHeight);
    int bandSelectControlsTargetWidth = getWidth() / 2;
    if (bandSelectControlsTargetWidth < 150) bandSelectControlsTargetWidth = 150;
    bandSelectControls.setBounds(
        bandSelectArea.getCentreX() - bandSelectControlsTargetWidth / 2,
        bandSelectArea.getY(),
        bandSelectControlsTargetWidth,
        bandSelectHeight
    );

    bounds.removeFromTop(padding * 2 + smallPadding); // Увеличенный отступ перед роторными ручками

    // 5. Секция роторных ручек (СНАЧАЛА РЕВЕРБ, ПОТОМ PAN ПОД НИМИ)
    // Область для всех ручек реверба и панорамы
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
            // Выделяем область для всего компонента RotarySliderWithLabels
            slider.setBounds(currentPlacementArea.removeFromLeft(rotarySliderWidth).withHeight(rotarySliderTotalHeight));
            currentPlacementArea.removeFromLeft(padding);
        }
        };

    placeRotary(wetSlider, reverbKnobsRowArea);
    placeRotary(spaceSlider, reverbKnobsRowArea);
    placeRotary(distanceSlider, reverbKnobsRowArea);
    placeRotary(delaySlider, reverbKnobsRowArea);

    rotaryMasterArea.removeFromTop(padding);
    // --- Ручка Pan ПОД ручками реверба и отцентрирована ---
    // --- Ручка Pan и ее внешняя метка ---
    // Высота для внешней метки Pan
    const int panExternalLabelHeight = panLabel.getFont().getHeight();
    // Общая высота для Pan слайдера (с его внутренним заголовком) и внешней метки под ним
    const int panControlTotalHeight = rotarySliderTotalHeight + smallPadding + panExternalLabelHeight;

    if (rotaryMasterArea.getHeight() >= panControlTotalHeight) {
        auto panControlArea = rotaryMasterArea.removeFromTop(panControlTotalHeight);

        // Центрируем область Pan
        int panAreaX = panControlArea.getCentreX() - rotarySliderWidth / 2;
        auto singlePanArea = juce::Rectangle<int>(panAreaX, panControlArea.getY(), rotarySliderWidth, panControlArea.getHeight());

        // Pan слайдер (он сам нарисует свой заголовок "Pan", если он установлен в конструкторе RotarySliderWithLabels)
        panSlider.setBounds(singlePanArea.removeFromTop(rotarySliderTotalHeight));
        singlePanArea.removeFromTop(smallPadding);
        // Внешняя метка "Pan" (возможно, она теперь не нужна, если RotarySliderWithLabels рисует свой заголовок)
        // Если вы хотите внешнюю метку, раскомментируйте и настройте ее.
        // panLabel.setBounds(singlePanArea);
        // Если panSlider.setName("Pan") был установлен, то внешняя panLabel не нужна.
        // Если вы хотите, чтобы panSlider не имел внутреннего заголовка, а использовал внешнюю panLabel:
        // 1. В конструкторе panSlider: panSlider(nullptr, "", ""), // Пустой title
        // 2. Здесь: panLabel.setBounds(singlePanArea.removeFromTop(panExternalLabelHeight));
        //            panSlider.setBounds(singlePanArea); // Оставшееся место для самого слайдера без внутреннего title
        // Пока оставим как есть: panSlider будет иметь внутренний title "Pan", а panLabel невидима или удалена.
        panLabel.setVisible(false); // Скрываем внешнюю panLabel, т.к. RotarySliderWithLabels имеет свой title
    }
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

void MBRPAudioProcessorEditor::updateReverbAttachments(int bandIndex) {
    juce::String wetParamId, spaceParamId, distanceParamId, delayParamId;
    juce::Colour bandColour;

    switch (bandIndex) {
    case 0: // Low
        wetParamId = "lowWet"; spaceParamId = "lowSpace"; distanceParamId = "lowDistance"; delayParamId = "lowDelay";
        bandColour = ColorScheme::getLowBandColor();
        break;
    case 1: // Mid
        wetParamId = "midWet"; spaceParamId = "midSpace"; distanceParamId = "midDistance"; delayParamId = "midDelay";
        bandColour = ColorScheme::getMidBandColor();
        break;
    case 2: // High
        wetParamId = "highWet"; spaceParamId = "highSpace"; distanceParamId = "highDistance"; delayParamId = "highDelay";
        bandColour = ColorScheme::getHighBandColor();
        break;
    default:
        jassertfalse; // Недопустимый индекс
        return;
    }

    auto updateSingleReverbSlider = [&](RotarySliderWithLabels& slider, const juce::String& paramID, std::unique_ptr<SliderAttachment>& attachment) {
        auto* targetParam = processorRef.getAPVTS().getParameter(paramID);
        auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
        jassert(rangedParam != nullptr);
        if (!rangedParam) return;

        slider.changeParam(rangedParam); // Обновляем параметр в RotarySliderWithLabels
        attachment.reset();
        attachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramID, slider);

        slider.setColour(juce::Slider::thumbColourId, bandColour);
        slider.setColour(juce::Slider::rotarySliderFillColourId, bandColour.withAlpha(0.7f));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, bandColour.darker(0.3f));
        slider.repaint();
        };

    updateSingleReverbSlider(wetSlider, wetParamId, wetAttachment);
    updateSingleReverbSlider(spaceSlider, spaceParamId, spaceAttachment);
    updateSingleReverbSlider(distanceSlider, distanceParamId, distanceAttachment);
    updateSingleReverbSlider(delaySlider, delayParamId, delayAttachment);
}
