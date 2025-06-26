#pragma once

#include <JuceHeader.h>
#include "LookAndFeel.h" // Для ColorScheme, если он там

namespace MBRP_GUI
{
    struct BandSelectControls : juce::Component
    {
        BandSelectControls();
        ~BandSelectControls() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override; // paint может быть не нужен, если кнопки полностью перекрывают фон

        // Теперь 4 кнопки
        juce::ToggleButton lowBandButton;
        juce::ToggleButton lowMidBandButton;    // Новая
        juce::ToggleButton midHighBandButton;   // Бывшая midBandButton
        juce::ToggleButton highBandButton;

        std::function<void(int bandIndex)> onBandSelected; // bandIndex 0..3

    private:
        void bandButtonClicked(juce::Button* button);
        juce::ToggleButton* activeBandButton = nullptr; // Инициализируем в конструкторе
    };
}
