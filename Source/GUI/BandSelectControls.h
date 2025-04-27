#pragma once

#include <JuceHeader.h>
#include "LookAndFeel.h" // Для стилизации и цветов

namespace MBRP_GUI // Используем наше пространство имен
{

    // Простой компонент для выбора активной полосы (НЧ, СЧ, ВЧ)
    struct BandSelectControls : juce::Component
    {
        BandSelectControls(); // Конструктор
        ~BandSelectControls() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override;

        // Кнопки выбора полосы
        juce::ToggleButton lowBandButton, midBandButton, highBandButton;

        // Делегат, чтобы уведомить редактор о смене полосы ПРИ КЛИКЕ НА КНОПКУ
        std::function<void(int bandIndex)> onBandSelected; // 0=Low, 1=Mid, 2=High

        // --- ДОБАВЛЕНО: Метод для программной установки выбранной полосы ---
        void setSelectedBand(int bandIndex);
        // ------------------------------------------------------------------

    private:
        // Внутренний обработчик кликов по кнопкам
        void bandButtonClicked(juce::Button* button);

        // Указатель на текущую активную кнопку (для стилизации)
        juce::ToggleButton* activeBandButton = &lowBandButton;
    };

} // Конец namespace MBRP_GUI