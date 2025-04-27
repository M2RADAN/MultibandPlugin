#include "BandSelectControls.h"
// #include "Utilities.h" // Не используется здесь напрямую

namespace MBRP_GUI
{

    BandSelectControls::BandSelectControls()
    {
        // Лямбда для настройки кнопок выбора полосы
        auto setupBandButton = [&](juce::ToggleButton& button, const juce::String& name)
            {
                button.setName(name);
                button.setButtonText(name);
                // Используем LookAndFeel, который применит стили
                button.setRadioGroupId(1); // Объединяем в группу
                button.setClickingTogglesState(true); // Переключать по клику
                button.onClick = [this, &button]() { bandButtonClicked(&button); }; // Обработчик клика
                addAndMakeVisible(button);
            };

        // Настраиваем кнопки
        setupBandButton(lowBandButton, "Low");
        setupBandButton(midBandButton, "Mid");
        setupBandButton(highBandButton, "High");

        // Выбираем "Low" по умолчанию
        lowBandButton.setToggleState(true, juce::NotificationType::dontSendNotification);
        activeBandButton = &lowBandButton;
    }

    void BandSelectControls::resized()
    {
        auto bounds = getLocalBounds().reduced(5);
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::row;
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;

        flexBox.items.add(juce::FlexItem(lowBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(midBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(highBandButton).withFlex(1.0f));

        flexBox.performLayout(bounds);
    }

    void BandSelectControls::paint(juce::Graphics& g)
    {
        // Можно оставить пустым или нарисовать фон, если нужно
        g.fillAll(juce::Colours::transparentBlack);
    }

    // Обработчик кликов по кнопкам выбора полосы
    void BandSelectControls::bandButtonClicked(juce::Button* button)
    {
        int bandIndex = -1;
        if (button == &lowBandButton) { bandIndex = 0; activeBandButton = &lowBandButton; }
        else if (button == &midBandButton) { bandIndex = 1; activeBandButton = &midBandButton; }
        else if (button == &highBandButton) { bandIndex = 2; activeBandButton = &highBandButton; }

        // Уведомляем подписчика (редактор) ТОЛЬКО при реальном клике
        if (bandIndex != -1 && onBandSelected)
        {
            onBandSelected(bandIndex);
        }
        // Перерисовываем для обновления вида кнопок LookAndFeel'ом
        repaint();
    }

    // --- ДОБАВЛЕНО: Реализация setSelectedBand ---
    // Этот метод вызывается извне (из редактора), чтобы программно
    // выбрать нужную кнопку, например, после клика на анализаторе.
    void BandSelectControls::setSelectedBand(int bandIndex)
    {
        juce::ToggleButton* buttonToSelect = nullptr;
        switch (bandIndex)
        {
        case 0: buttonToSelect = &lowBandButton; break;
        case 1: buttonToSelect = &midBandButton; break;
        case 2: buttonToSelect = &highBandButton; break;
        default: return; // Неверный индекс, ничего не делаем
        }

        // Меняем состояние, только если выбрана другая кнопка
        if (buttonToSelect != nullptr && buttonToSelect != activeBandButton)
        {
            // Устанавливаем состояние новой кнопки БЕЗ вызова onClick
            // и БЕЗ отправки уведомлений слушателям этой кнопки
            buttonToSelect->setToggleState(true, juce::NotificationType::dontSendNotification);

            // activeBandButton обновится автоматически, так как setToggleState(true)
            // внутри радио-группы сбросит состояние других кнопок.
            // Но для надежности можно обновить и вручную:
            activeBandButton = buttonToSelect;

            // Перерисовываем компонент, чтобы LookAndFeel обновил вид кнопок
            repaint();
        }
    }
    // -------------------------------------------

} // Конец namespace MBRP_GUI