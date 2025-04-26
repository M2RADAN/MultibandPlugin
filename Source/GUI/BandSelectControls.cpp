#include "BandSelectControls.h"
#include "Utilities.h" // Для drawModuleBackground и mapX/mapY, если нужны

namespace MBRP_GUI
{

    BandSelectControls::BandSelectControls()
    {
        // Лямбда для настройки кнопок выбора полосы
        auto setupBandButton = [&](juce::ToggleButton& button, const juce::String& name, juce::Colour /*color*/) // Цвет пока не используем напрямую
            {
                button.setName(name);
                button.setButtonText(name); // Устанавливаем текст кнопки
                // Используем стандартный LookAndFeel для ToggleButton, который мы настроим в LookAndFeel.cpp
                // (стилизация будет применена через LookAndFeel::drawToggleButton)
                button.setRadioGroupId(1); // Объединяем в группу радио-кнопок
                button.setClickingTogglesState(true); // Переключать состояние по клику
                button.onClick = [this, &button]() { bandButtonClicked(&button); }; // Обработчик клика
                addAndMakeVisible(button);
            };

        // Настраиваем кнопки
        setupBandButton(lowBandButton, "Low", juce::Colours::orange); // Цвет передается, но не используется напрямую здесь
        setupBandButton(midBandButton, "Mid", juce::Colours::lightgreen);
        setupBandButton(highBandButton, "High", juce::Colours::cyan);

        // Выбираем "Low" по умолчанию
        lowBandButton.setToggleState(true, juce::NotificationType::dontSendNotification);
        activeBandButton = &lowBandButton; // Устанавливаем активную кнопку
    }

    void BandSelectControls::resized()
    {
        auto bounds = getLocalBounds().reduced(5); // Небольшой отступ
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::row; // Располагаем в ряд
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // Распределяем равномерн
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch; // Растягиваем по высоте

        // Добавляем кнопки во FlexBox
        flexBox.items.add(juce::FlexItem(lowBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(midBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(highBandButton).withFlex(1.0f));

        // Применяем макет
        flexBox.performLayout(bounds);
    }

    void BandSelectControls::paint(juce::Graphics& g)
    {
        // Опционально: рисуем фон модуля, если нужно
        // MBRP_GUI::drawModuleBackground(g, getLocalBounds());
        g.fillAll(juce::Colours::transparentBlack); // Прозрачный фон по умолчанию
    }

    // Обработчик кликов по кнопкам выбора полосы
    void BandSelectControls::bandButtonClicked(juce::Button* button)
    {
        // Определяем индекс выбранной полосы
        int bandIndex = -1;
        if (button == &lowBandButton)
        {
            bandIndex = 0;
            activeBandButton = &lowBandButton;
        }
        else if (button == &midBandButton)
        {
            bandIndex = 1;
            activeBandButton = &midBandButton;
        }
        else if (button == &highBandButton)
        {
            bandIndex = 2;
            activeBandButton = &highBandButton;
        }

        // Если есть подписчик (редактор) и индекс корректен, уведомляем его
        if (bandIndex != -1 && onBandSelected)
        {
            onBandSelected(bandIndex);
        }
        // Перерисовываем, чтобы LookAndFeel обновил вид кнопок
        repaint();
    }

} // Конец namespace MBRP_GUI