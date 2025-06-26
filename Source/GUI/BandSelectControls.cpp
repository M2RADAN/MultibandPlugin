#include "BandSelectControls.h"
#include "Utilities.h" // Не используется здесь напрямую, но может быть полезен для других вещей
#include "LookAndFeel.h" // Для ColorScheme, если кнопки используют цвета оттуда

namespace MBRP_GUI
{
    BandSelectControls::BandSelectControls()
    {
        // Настройка кнопок выбора полосы
        auto setupBandButton = [&](juce::ToggleButton& button, const juce::String& name, const juce::String& text)
        {
            button.setName(name);         // Внутреннее имя для отладки/автоматизации
            button.setButtonText(text);   // Текст на кнопке
            button.setRadioGroupId(1);    // Все кнопки в одной радио-группе
            button.setClickingTogglesState(true);
            button.onClick = [this, &button]() { bandButtonClicked(&button); };
            //button.setColour(juce::Button::Textboxcolour);
            addAndMakeVisible(button);
        };

        // Имена и тексты для 4-х полос
        setupBandButton(lowBandButton, "LowBand", "Low");
        setupBandButton(lowMidBandButton, "LowMidBand", "L-Mid");
        setupBandButton(midHighBandButton, "MidHighBand", "M-High");
        setupBandButton(highBandButton, "HighBand", "High");

        // Устанавливаем начальное состояние (например, выбрана "Low")
        lowBandButton.setToggleState(true, juce::NotificationType::dontSendNotification);
        activeBandButton = &lowBandButton;
    }

    void BandSelectControls::resized()
    {
        auto bounds = getLocalBounds().reduced(2); // Небольшой отступ по краям
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::row;
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;

        // Добавляем все 4 кнопки в FlexBox
        flexBox.items.add(juce::FlexItem(lowBandButton).withFlex(1.0f).withMargin(1));
        flexBox.items.add(juce::FlexItem(lowMidBandButton).withFlex(1.0f).withMargin(1));
        flexBox.items.add(juce::FlexItem(midHighBandButton).withFlex(1.0f).withMargin(1));
        flexBox.items.add(juce::FlexItem(highBandButton).withFlex(1.0f).withMargin(1));

        flexBox.performLayout(bounds);
    }

    void BandSelectControls::paint(juce::Graphics& g)
    {
        // Обычно фон не рисуется, так как кнопки его перекрывают.
        // Если нужен фон под кнопками (например, если между ними есть зазоры), раскомментируйте:
        // g.fillAll(ColorScheme::getBackgroundColor().darker(0.1f)); 
        juce::ignoreUnused(g);
    }

    void BandSelectControls::bandButtonClicked(juce::Button* button)
    {
        int bandIndex = -1;
        if (button == &lowBandButton) { bandIndex = 0; activeBandButton = &lowBandButton; }
        else if (button == &lowMidBandButton) { bandIndex = 1; activeBandButton = &lowMidBandButton; }
        else if (button == &midHighBandButton) { bandIndex = 2; activeBandButton = &midHighBandButton; }
        else if (button == &highBandButton) { bandIndex = 3; activeBandButton = &highBandButton; }

        if (bandIndex != -1 && onBandSelected)
        {
            onBandSelected(bandIndex);
        }
        // repaint(); // LookAndFeel::drawToggleButton позаботится о перерисовке кнопки
    }
}
