#include "PluginProcessor.h"
#include "PluginEditor.h" // Включает MBRPAudioProcessorEditor и зависимости GUI

// --- Реализация ControlBar ---
ControlBar::ControlBar()
{
    // Инициализация кнопки анализатора
    analyzerButton.setToggleState(true, juce::NotificationType::dontSendNotification); // Включена по умолчанию
    addAndMakeVisible(analyzerButton); // Добавляем на экран
}

void ControlBar::resized()
{
    // Расположение кнопки анализатора слева
    auto bounds = getLocalBounds();
    analyzerButton.setBounds(bounds.removeFromLeft(50) // Ширина кнопки
        .withTrimmedTop(4)      // Небольшой отступ сверху
        .withTrimmedLeft(4));   // Небольшой отступ слева
}
// ---------------------------


//==============================================================================
MBRPAudioProcessorEditor::MBRPAudioProcessorEditor(MBRPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    // Инициализация компонентов GUI
    // Передаем процессор и ссылки на FIFO в анализатор
    analyzer(p, p.leftChannelFifo, p.rightChannelFifo),
    // Передаем ссылки на параметры кроссовера в оверлей
    analyzerOverlay(*p.lowMidCrossover, *p.midHighCrossover),
    panSlider(nullptr, "", "Pan"),
    // Инициализация Attachments для слайдеров кроссоверов
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider)
    // Аттачмент для панорамы panAttachment инициализируется в конце конструктора
{
    setLookAndFeel(&lnf); // Устанавливаем кастомный LookAndFeel

    // Подключаем обработчик для кнопки вкл/выкл анализатора
    controlBar.analyzerButton.onClick = [this]()
        {
            analyzer.toggleAnalysisEnablement(controlBar.analyzerButton.getToggleState());
        };

    // Добавляем все основные компоненты GUI на экран
    addAndMakeVisible(controlBar);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(analyzerOverlay); // Overlay рисуется поверх анализатора
    addAndMakeVisible(bandSelectControls); // Кнопки выбора полосы

    // Лямбда для настройки внешнего вида слайдеров
    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText,
        juce::Slider::SliderStyle style, juce::Colour colour)
        {
            slider.setSliderStyle(style);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20); // Текстбокс для линейных
            slider.setPopupDisplayEnabled(true, false, this);
            slider.setColour(juce::Slider::thumbColourId, colour);
            slider.setColour(juce::Slider::trackColourId, colour.darker(0.5f)); // Цвет трека
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
    // --- ИЗМЕНЕНО: Настройка ОДНОГО кастомного слайдера панорамы ---
    // Основная настройка вида (цвета, текст) будет происходить в updatePanAttachment
    // Здесь просто делаем его видимым и настраиваем внешний лейбл
    addAndMakeVisible(panSlider); // Делаем видимым RotarySliderWithLabels
    panLabel.setText("Pan", juce::dontSendNotification); // Начальный текст лейбла
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.attachToComponent(&panSlider, false); // Прикрепляем внешний лейбл под ним
    panLabel.setFont(juce::Font(12.0f));
    panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(panLabel); // Делаем видимым внешний лейбл
    // ---------------------------------------------------------------

    // Подключаем обработчик выбора полосы
    bandSelectControls.onBandSelected = [this](int bandIndex) {
        updatePanAttachment(bandIndex);
        };

    // Инициализируем аттачмент и вид панорамы для начальной полосы (Low)
    updatePanAttachment(0);

    setSize(600, 700);
    startTimerHz(60);
}


MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor()
{
    setLookAndFeel(nullptr); // Сбрасываем LookAndFeel при удалении редактора
}

//==============================================================================
void MBRPAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Заливаем фон цветом из LookAndFeel
    g.fillAll(lnf.findColour(juce::ResizableWindow::backgroundColourId));
    // Здесь можно добавить код для рисования кастомного заголовка или фона,
    // скопировав его из SimpleMBComp::paint и адаптировав цвета/шрифты
}

void MBRPAudioProcessorEditor::resized()
{
    // Расположение всех компонентов внутри окна редактора
    auto bounds = getLocalBounds(); // Границы всего окна
    int padding = 10; // Стандартный отступ

    // ControlBar сверху
    controlBar.setBounds(bounds.removeFromTop(32));

    // Анализатор спектра занимает верхнюю часть оставшегося пространства
    int analyzerHeight = 250; // Задаем высоту анализатора
    auto analyzerArea = bounds.removeFromTop(analyzerHeight);
    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzerArea); // Оверлей рисуется точно над анализатором

    // Оставшаяся область для остальных контролов с отступами
    auto controlsArea = bounds.reduced(padding);
    controlsArea.removeFromTop(padding); // Дополнительный отступ сверху

    // Слайдеры кроссоверов
    int crossoverHeight = 50; // Высота области кроссоверов
    auto crossoverArea = controlsArea.removeFromTop(crossoverHeight);
    int crossoverWidth = crossoverArea.getWidth() / 2; // Делим пополам
    lowMidCrossoverSlider.setBounds(crossoverArea.removeFromLeft(crossoverWidth).reduced(padding / 2));
    midHighCrossoverSlider.setBounds(crossoverArea.reduced(padding / 2));

    controlsArea.removeFromTop(padding); // Отступ

    // Кнопки выбора полосы
    int bandSelectHeight = 30; // Высота области кнопок
    bandSelectControls.setBounds(controlsArea.removeFromTop(bandSelectHeight));

    controlsArea.removeFromTop(padding / 2); // Отступ

    // Единственный слайдер панорамы
    auto panArea = controlsArea;
    // Размещаем его, например, по центру с отступами
    panSlider.setBounds(panArea.reduced(panArea.getWidth() / 4, padding));

    // Лейблы автоматически размещаются под своими слайдерами
    // благодаря вызову label.attachToComponent(&slider, false);
}

void MBRPAudioProcessorEditor::timerCallback()
{
    // Этот таймер может использоваться для обновления кастомных элементов UI,
    // которые не обновляются автоматически (например, RMS метры).
    // Анализатор и Overlay имеют свои механизмы обновления через таймеры или PathProducer.
    // Пока оставляем пустым.
}

// --- Функция обновления аттачмента и вида слайдера панорамы ---
void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex)
{
    juce::String paramId;       // ID параметра панорамы для этой полосы
    juce::String labelText;     // Текст для лейбла под слайдером
    juce::Colour color;         // Цвет для слайдера

    // Определяем ID, текст и цвет в зависимости от выбранной полосы
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
        jassertfalse; // Недопустимый индекс полосы
        return;
    }

    // --- ИЗМЕНЕНО: Работаем с RotarySliderWithLabels ---
   // 1. Получаем указатель на целевой параметр
    auto* targetParam = processorRef.getAPVTS().getParameter(paramId);
    // Динамическое приведение типа не обязательно, т.к. changeParam принимает RangedAudioParameter
    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
    jassert(rangedParam != nullptr); // Убедимся, что параметр найден и имеет диапазон
    if (!rangedParam) return;

    // 2. Обновляем параметр, с которым работает RotarySliderWithLabels
    //    и его внутренние метки/суффикс (суффикс не нужен для панорамы)
    panSlider.changeParam(rangedParam); // Передаем новый параметр
    panSlider.labels.clear();
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);// Очищаем старые метки (если были)
    // Добавляем метки для -1 (L), 0 (C), +1 (R)
    panSlider.labels.add({ 0.0f, "L" });   // Позиция 0.0 соответствует значению -1.0
    panSlider.labels.add({ 0.5f, "C" });   // Позиция 0.5 соответствует значению 0.0
    panSlider.labels.add({ 1.0f, "R" });   // Позиция 1.0 соответствует значению +1.0
    // 1. Удаляем старый аттачмент (если он был)
    panAttachment.reset();
    // 2. Создаем новый аттачмент, связывая panSlider с выбранным paramId
    panAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramId, panSlider);

    // 3. Обновляем внешний вид слайдера и лейбла
    panLabel.setText(labelText, juce::dontSendNotification);
    panSlider.setColour(juce::Slider::thumbColourId, color);
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, color.withAlpha(0.7f));
    panSlider.repaint(); // Принудительно перерисовываем слайдер с новым цветом
}
// ----------------------------------------------------------------------
