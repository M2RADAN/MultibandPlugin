
#pragma once

#include <JuceHeader.h>

struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter* rap,
        const juce::String& unitSuffix,
        const juce::String& title /*= "NO TITLE"*/) :
        juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
            juce::Slider::TextEntryBoxPosition::NoTextBox),
        param(rap),
        suffix(unitSuffix),
        titleAboveSlider(true) // По умолчанию заголовок сверху
    {
        setName(title);
        // Устанавливаем шрифт по умолчанию, если он не будет переопределен в LookAndFeel для заголовка
        // titleFont = juce::Font ("K2D", 24.0f, juce::Font::plain); // Если шрифт не найден, будет использован системный
    }

    struct LabelPos
    {
        float pos;
        juce::String label;
    };

    juce::Array<LabelPos> labels; // Метки диапазона (0%, 100% и т.д.)
    bool drawRangeLabels = true; // Флаг для отображения меток диапазона

    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    // int getTextHeight() const { return 14; } // Старая высота для заголовка
    int getTitleHeight() const { return 28; } // Новая высота для заголовка под слайдером (24 шрифт + отступы)
    virtual juce::String getDisplayString() const;

    void changeParam(juce::RangedAudioParameter* p);

    // --- НОВЫЕ МЕТОДЫ ---
    void setTitlePosition(bool above) { titleAboveSlider = above; }
    void setShowRangeLabels(bool show) { drawRangeLabels = show; }
    // --------------------

protected:
    juce::RangedAudioParameter* param;
    juce::String suffix;
    bool titleAboveSlider;
    // juce::Font titleFont; // Можно хранить шрифт здесь или получать его в paint
};

struct RatioSlider : RotarySliderWithLabels
{
    RatioSlider(juce::RangedAudioParameter* rap,
                const juce::String& unitSuffix) :
    RotarySliderWithLabels(rap, unitSuffix, "RATIO") {}
    
    juce::String getDisplayString() const override;
};
