#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- ControlBar Implementation ---
ControlBar::ControlBar() {
    addAndMakeVisible(analyzerButton);
}
void ControlBar::resized() {
    auto bounds = getLocalBounds();
    analyzerButton.setBounds(bounds.removeFromLeft(50).withTrimmedTop(4).withTrimmedLeft(4));
    // TODO: Добавить функционал кнопке analyzerButton
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

    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20);
        slider.setPopupDisplayEnabled(true, false, this);
        slider.setColour(juce::Slider::thumbColourId, ColorScheme::getSliderThumbColor());
        slider.setColour(juce::Slider::trackColourId, ColorScheme::getSliderTrackColor());
        // --- Устанавливаем цвет текста в TextBox ---
        slider.setColour(juce::Slider::textBoxTextColourId, ColorScheme::getSecondaryTextColor()); // Используем менее яркий серый цвет
        // Устанавливаем цвет рамки TextBox (можно сделать чуть темнее фона)
        slider.setColour(juce::Slider::textBoxOutlineColourId, ColorScheme::getSliderTrackColor().darker(0.2f));
        // Цвет фона TextBox можно оставить по умолчанию или установить явно
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
    // Устанавливаем цвета роторного слайдера через LookAndFeel ID
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, ColorScheme::getSliderFillColor());
    panSlider.setColour(juce::Slider::rotarySliderOutlineColourId, ColorScheme::getSliderBorderColor());
    panSlider.setColour(juce::Slider::thumbColourId, ColorScheme::getSliderThumbColor()); // Цвет указателя
    addAndMakeVisible(panSlider);

    panLabel.setText("Pan", juce::dontSendNotification); // Устанавливаем текст "Pan"
    panLabel.setJustificationType(juce::Justification::centred);
    // panLabel.attachToComponent(&panSlider, false); // Не прикрепляем, разместим вручную
    panLabel.setFont(juce::Font(14.0f, juce::Font::bold)); // Делаем метку "Pan" заметнее
    panLabel.setColour(juce::Label::textColourId, ColorScheme::getDarkTextColor()); // Используем более темный цвет
    addAndMakeVisible(panLabel);

    bandSelectControls.onBandSelected = [this](int bandIndex) { updatePanAttachment(bandIndex); };
    analyzerOverlay.onBandAreaClicked = [this](int bandIndex) { handleBandAreaClick(bandIndex); }; // <-- Установлен колбэк
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
    int controlBarHeight = 32; // Убрал, если ControlBar не используется активно
    // controlBar.setBounds(bounds.removeFromTop(controlBarHeight));

    int analyzerHeight = 300; // Немного уменьшим анализатор
    auto analyzerArea = bounds.removeFromTop(analyzerHeight).reduced(padding, 0); // Добавим отступы по бокам
    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzerArea);

    // Область под анализатором
    auto controlsArea = bounds.reduced(padding);
    controlsArea.removeFromTop(padding); // Отступ сверху

    // Слайдеры кроссоверов
    int labelHeight = 15;
    int sliderHeight = 40;
    int crossoverControlHeight = labelHeight + sliderHeight;
    auto crossoverArea = controlsArea.removeFromTop(crossoverControlHeight);
    int crossoverWidth = crossoverArea.getWidth() / 2 - padding / 2;

    auto lowMidArea = crossoverArea.removeFromLeft(crossoverWidth);
    lowMidCrossoverLabel.setBounds(lowMidArea.removeFromTop(labelHeight));
    lowMidCrossoverSlider.setBounds(lowMidArea);

    crossoverArea.removeFromLeft(padding); // Отступ между слайдерами

    auto midHighArea = crossoverArea;
    midHighCrossoverLabel.setBounds(midHighArea.removeFromTop(labelHeight));
    midHighCrossoverSlider.setBounds(midHighArea);

    controlsArea.removeFromTop(padding); // Отступ

    // Кнопки выбора полосы
    int bandSelectHeight = 30;
    bandSelectControls.setBounds(controlsArea.removeFromTop(bandSelectHeight).reduced(crossoverWidth / 2, 0)); // Центрируем кнопки

    controlsArea.removeFromTop(padding * 2); // Больший отступ перед Pan

    // Область Pan
    auto panArea = controlsArea;
    int panLabelHeight = 20; // Высота для метки "Pan"
    // Размещаем метку "Pan" ВНИЗУ области panArea
    panLabel.setBounds(panArea.removeFromBottom(panLabelHeight));
    // Оставшаяся область для слайдера, центрируем его
    panSlider.setBounds(panArea.reduced(panArea.getWidth() / 3, padding)); // Делаем слайдер покрупнее и центрируем
}

void MBRPAudioProcessorEditor::timerCallback() {
   
}

void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex) {
    juce::String paramId;
    // juce::String labelText; // Больше не нужен специфичный текст метки
    juce::Colour bandColour; // Цвет для слайдера

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

    panSlider.changeParam(rangedParam); // Обновляем параметр слайдера

    // --- Убираем настройку текстового поля и внутренних меток ---
    panSlider.labels.clear(); // Очищаем старые (на всякий случай)
    panSlider.labels.add({ 0.0f, "L" }); // 0.0 соответствует значению -1.0 панорамы
    panSlider.labels.add({ 0.5f, "C" }); // 0.5 соответствует значению 0.0 панорамы
    panSlider.labels.add({ 1.0f, "R" }); // 1.0 соответствует значению +1.0 панорамы
    // -----------------------------------------

    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // Текстовое поле все еще не нужно

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
    case 1: buttonToSelect = &bandSelectControls.midBandButton; break;
    case 2: buttonToSelect = &bandSelectControls.highBandButton; break;
    default:
        jassertfalse; // Недопустимый индекс
        return;
    }

    // Проверяем, не выбрана ли уже эта кнопка
    if (buttonToSelect && !buttonToSelect->getToggleState())
    {
        // Вызываем setToggleState с уведомлением.
        // Это должно запустить onClick в BandSelectControls, который вызовет onBandSelected,
        // который, в свою очередь, вызовет updatePanAttachment.
        // sendNotificationSync гарантирует, что это произойдет немедленно в потоке сообщений.
        buttonToSelect->setToggleState(true, juce::sendNotificationSync);

        // Напрямую вызывать updatePanAttachment здесь не нужно,
        // так как это сделает колбэк onBandSelected из bandSelectControls.
    }
}