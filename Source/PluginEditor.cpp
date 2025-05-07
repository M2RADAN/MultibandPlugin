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

    // --- НОВОЕ: Настройка слайдеров реверба ---
    auto setupReverbSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText, const juce::String& suffix = "") {
        slider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
        slider.setPopupDisplayEnabled(true, false, this);
        slider.setTextValueSuffix(suffix);
        // Установка цветов через LookAndFeel
        slider.setColour(juce::Slider::rotarySliderFillColourId, ColorScheme::getSliderFillColor());
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, ColorScheme::getSliderBorderColor());
        slider.setColour(juce::Slider::thumbColourId, ColorScheme::getSliderThumbColor());
        slider.setColour(juce::Slider::textBoxTextColourId, ColorScheme::getSecondaryTextColor());
        slider.setColour(juce::Slider::textBoxOutlineColourId, ColorScheme::getSliderTrackColor().darker(0.2f));
        slider.setName(labelText); // Имя для отладки/автоматизации
        addAndMakeVisible(slider);

        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.attachToComponent(&slider, false); // Привязываем метку снизу
        label.setFont(juce::Font(12.0f));
        label.setColour(juce::Label::textColourId, ColorScheme::getTextColor());
        addAndMakeVisible(label);
    };

    setupReverbSlider(wetSlider, wetLabel, "Wet", " %");
    setupReverbSlider(spaceSlider, spaceLabel, "Space", " %");
    setupReverbSlider(distanceSlider, distanceLabel, "Distance", " %");
    setupReverbSlider(delaySlider, delayLabel, "Pre-Delay", " ms");
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
    const int rotaryKnobVisualSize = 80; // Фактический диаметр видимой части ручки (чуть меньше области)
    const int rotaryKnobHeightWithText = rotaryKnobVisualSize + smallPadding + rotaryLabelHeight; // Высота ручки + отступ + высота текстбокса

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

    bounds.removeFromTop(padding * 2); // Увеличенный отступ перед роторными ручками

    // 5. Секция роторных ручек (СНАЧАЛА РЕВЕРБ, ПОТОМ PAN ПОД НИМИ)
    // Область для всех ручек реверба и панорамы
    auto rotaryMasterArea = bounds.reduced(padding, 0);

    // --- Ручки реверба в ряд ---
    int numReverbKnobs = 4;
    // Общая ширина, необходимая для ручек реверба и отступов между ними
    int totalReverbKnobsWidth = numReverbKnobs * rotaryKnobWidth + (numReverbKnobs - 1) * padding;
    // Начальная X координата, чтобы отцентрировать группу ручек реверба
    int reverbKnobsStartX = rotaryMasterArea.getX() + (rotaryMasterArea.getWidth() - totalReverbKnobsWidth) / 2;

    // Область для текущего ряда ручек реверба
    auto reverbKnobsRowArea = rotaryMasterArea.removeFromTop(rotaryLabelHeight + rotaryKnobHeightWithText);
    reverbKnobsRowArea.setX(reverbKnobsStartX);
    reverbKnobsRowArea.setWidth(totalReverbKnobsWidth);


    auto placeReverbRotary = [&](juce::Slider& slider, juce::Label& label, juce::Rectangle<int>& currentKnobPlacementArea) {
        if (currentKnobPlacementArea.getWidth() >= rotaryKnobWidth) {
            auto singleKnobArea = currentKnobPlacementArea.removeFromLeft(rotaryKnobWidth);

            // Метка НАД слайдером
            label.setBounds(singleKnobArea.getX() + (rotaryKnobWidth - rotaryKnobVisualSize) / 2, // Центрируем метку над видимой частью ручки
                singleKnobArea.getY(),
                rotaryKnobVisualSize, // Ширина метки = диаметру ручки
                rotaryLabelHeight);
            // Сам слайдер (ручка + текстовое поле под ней)
            // Центрируем область слайдера внутри выделенной ему rotaryKnobWidth
            slider.setBounds(singleKnobArea.getX() + (rotaryKnobWidth - rotaryKnobVisualSize) / 2,
                singleKnobArea.getY() + rotaryLabelHeight,
                rotaryKnobVisualSize, // Ширина = диаметру ручки
                rotaryKnobHeightWithText - rotaryLabelHeight); // Высота = ручка + текстбокс

            currentKnobPlacementArea.removeFromLeft(padding); // Отступ до следующей ручки
        }
    };

    placeReverbRotary(wetSlider, wetLabel, reverbKnobsRowArea);
    placeReverbRotary(spaceSlider, spaceLabel, reverbKnobsRowArea);
    placeReverbRotary(distanceSlider, distanceLabel, reverbKnobsRowArea);
    placeReverbRotary(delaySlider, delayLabel, reverbKnobsRowArea);

    // Отступ после ручек реверба
    rotaryMasterArea.removeFromTop(padding);

    // --- Ручка Pan ПОД ручками реверба и отцентрирована ---
    if (rotaryMasterArea.getHeight() >= rotaryLabelHeight + rotaryKnobHeightWithText) {
        auto panKnobArea = rotaryMasterArea.removeFromTop(rotaryLabelHeight + rotaryKnobHeightWithText);

        // Центрируем область для Pan
        int panAreaX = panKnobArea.getCentreX() - rotaryKnobWidth / 2;

        panLabel.setBounds(panAreaX + (rotaryKnobWidth - rotaryKnobVisualSize) / 2, // Центрируем метку над видимой частью ручки
            panKnobArea.getY(),
            rotaryKnobVisualSize,
            rotaryLabelHeight);
        panSlider.setBounds(panAreaX + (rotaryKnobWidth - rotaryKnobVisualSize) / 2,
            panKnobArea.getY() + rotaryLabelHeight,
            rotaryKnobVisualSize,
            rotaryKnobHeightWithText - rotaryLabelHeight);
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

    // Сбрасываем старые аттачменты
    wetAttachment.reset();
    spaceAttachment.reset();
    distanceAttachment.reset();
    delayAttachment.reset();

    // Создаем новые аттачменты для выбранной полосы
    wetAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), wetParamId, wetSlider);
    spaceAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), spaceParamId, spaceSlider);
    distanceAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), distanceParamId, distanceSlider);
    delayAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), delayParamId, delaySlider);

    // Обновляем цвета слайдеров в соответствии с полосой
    auto updateSliderColour = [&](juce::Slider& slider) {
        slider.setColour(juce::Slider::thumbColourId, bandColour);
        slider.setColour(juce::Slider::rotarySliderFillColourId, bandColour.withAlpha(0.7f));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, bandColour.darker(0.3f));
        slider.repaint();
    };

    updateSliderColour(wetSlider);
    updateSliderColour(spaceSlider);
    updateSliderColour(distanceSlider);
    updateSliderColour(delaySlider);
}
