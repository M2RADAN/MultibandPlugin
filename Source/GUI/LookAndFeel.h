
#pragma once

#include <JuceHeader.h>

#define USE_LIVE_CONSTANT false

#if USE_LIVE_CONSTANT
#define colorHelper(c) JUCE_LIVE_CONSTANT(c);
#else
#define colorHelper(c) c;
#endif

namespace ColorScheme
{
    
    // --- Основные цвета светлой темы ---
    inline juce::Colour getBackgroundColor() { return colorHelper(juce::Colour(0xFF2D2D2D)); } // Основной фон редактора (светло-серый)
    inline juce::Colour getTextColor() { return colorHelper(juce::Colours::darkgrey); }   // Основной цвет текста
    inline juce::Colour getDarkTextColor() { return colorHelper(juce::Colours::black); }      // Для заголовков или акцентов
    inline juce::Colour getSecondaryTextColor() { return colorHelper(juce::Colours::grey); }       // Менее важный текст

    // --- Цвета анализатора (адаптированные под светлый фон) ---
    inline juce::Colour getAnalyzerBackgroundColor() { return colorHelper(juce::Colour(0xFF2D2D2D)); }     // Фон анализатора - белый
    inline juce::Colour getAnalyzerGridBaseColor() { return colorHelper(juce::Colour(0xFF4D4D4D)); } // Сетка анализатора
    inline juce::Colour getScaleTextColor() { return getTextColor(); }                       // Текст шкал (темно-серый)
    inline juce::Colour getAnalyzerPeakTextColor() { return getTextColor(); }                       // Текст пика
    inline juce::Colour getZeroDbLineBaseColor() { return colorHelper(juce::Colour(0xFF4D4D4D)); }  // Линия 0 дБ (темная)

    // --- Цвета спектра и линий (можно оставить яркими) ---
    inline juce::Colour getInputSignalColor() { return colorHelper(juce::Colour(0xff008cff)); } // Синий для спектра
    inline juce::Colour getSpectrumFillBaseColor() { return getInputSignalColor(); }
    inline juce::Colour getSpectrumLineColor() { return getInputSignalColor(); }
    inline juce::Colour getPeakHoldLineBaseColor() { return colorHelper(juce::Colours::orange); }    // Оранжевый для пиков
    inline juce::Colour getOverZeroDbLineColor() { return colorHelper(juce::Colours::red); }       // Красный для превышения 0 дБ

    // --- Цвета кроссоверов (оставляем) ---
    inline juce::Colour getLowBandColor() { return colorHelper(juce::Colour(255u, 154u, 1u)); }          // Orange (Low)
    inline juce::Colour getLowMidBandColor() { return colorHelper(juce::Colour(0xFF9ACD32)); }        // YellowGreen (Low-Mid)
    inline juce::Colour getMidBandColor() { return getLowMidBandColor(); } // Синоним для AnalyzerOverlay, если он ожидает "Mid"
    inline juce::Colour getMidHighBandColor() { return colorHelper(juce::Colours::lightgreen); }     // LightGreen (Mid-High)
    inline juce::Colour getHighBandAltColor() { return colorHelper(juce::Colours::cyan); }             // Cyan (High)

    // Для линий кроссовера в AnalyzerOverlay, если нужны отдельные
    inline juce::Colour getOrangeBorderColor() { return getLowBandColor(); } // Low/LowMid Crossover Line
    inline juce::Colour getMidCrossoverLineColor() { return getLowMidBandColor(); } // LowMid/MidHigh Crossover Line
    inline juce::Colour getMidHighCrossoverColor() { return getMidHighBandColor(); } // MidHigh/High Crossover Line (старое имя, но цвет для этой линии)


    // --- Цвета слайдеров ---
    inline juce::Colour getSliderFillColor() { return colorHelper(juce::Colours::lightgrey); }    // Заливка роторного слайдера
    inline juce::Colour getSliderTrackColor() { return colorHelper(juce::Colours::silver); }       // Трек линейного слайдера
    inline juce::Colour getSliderThumbColor() { return colorHelper(juce::Colours::cornflowerblue); } // Ручка слайдера (синяя)
    inline juce::Colour getSliderBorderColor() { return colorHelper(juce::Colours::darkgrey); }     // Граница роторного слайдера

    inline juce::Colour getRotarySliderBodyColor() { return colorHelper(juce::Colour(0xFF3D3D3D)); } // Темно-серый/синий фон слайдера
    inline juce::Colour getRotarySliderTrackColor() { return colorHelper(juce::Colour(0xFF2B2F38)); } // Чуть темнее для неактивного трека
    inline juce::Colour getRotarySliderBackBodyColor() { return colorHelper(juce::Colour(0xFF1E1E1E)); }
    inline juce::Colour getRotarySliderValueArcColor() { return colorHelper(juce::Colour(0xFF40E0D0)); } // Бирюзовый для дуги значения
    inline juce::Colour getRotarySliderThumbColor() { return getRotarySliderValueArcColor(); }    // Указатель того же цвета
    inline juce::Colour getRotarySliderTextColor() { return colorHelper(juce::Colours::whitesmoke); } // Белый/светлый текст на слайдере
    inline juce::Colour getRotarySliderLabelColor() { return getTextColor(); }

    // --- Цвета кнопок Low/Mid/High (BandSelectControls) ---
    inline juce::Colour getToggleButtonOffColor() { return colorHelper(juce::Colours::lightgrey); }    // Фон неактивной кнопки
    inline juce::Colour getToggleButtonOffBorder() { return colorHelper(juce::Colours::grey); }         // Граница неактивной кнопки
    inline juce::Colour getToggleButtonOffTextColor() { return getTextColor(); }                       // Текст неактивной кнопки (темно-серый)
    // Цвета для активной кнопки будут браться из цветов кроссоверов/полос
  //  inline juce::Colour getLowBandColor() { return getOrangeBorderColor(); }                 // Используем цвет Low/Mid кроссовера
  //  inline juce::Colour getMidBandColor() { return colorHelper(juce::Colours::lightgreen); } // Зеленый для Mid
  //  inline juce::Colour getHighBandColor() { return getMidHighCrossoverColor(); }             // Голубой для High
    inline juce::Colour getToggleButtonOnTextColor() { return colorHelper(juce::Colours::white); }       // Текст активной кнопки (белый)
    
    
    // --- Существующие цвета ---
    inline juce::Colour getGainReductionColor() { return colorHelper(juce::Colour(0xff38c19b)); }
    //inline juce::Colour getInputSignalColor() { return colorHelper(juce::Colour(0xff00e7ff)); } // Используем для линии спектра
    inline juce::Colour getOutputSignalColor() { return colorHelper(juce::Colour(0xff0cff00)); }
    //inline juce::Colour getSliderFillColor() { return colorHelper(juce::Colour(0xff96bbc2)); }
    //inline juce::Colour getOrangeBorderColor() { return colorHelper(juce::Colour(255u, 154u, 1u)); } // Используем для Low/Mid кроссовера
    inline juce::Colour getSliderRangeTextColor() { return colorHelper(juce::Colour(0u, 172u, 1u)); }
    //inline juce::Colour getSliderBorderColor() { return colorHelper(juce::Colour(0xff0087a2)); }
    inline juce::Colour getThresholdColor() { return colorHelper(juce::Colour(0xffe0760c)); }
    inline juce::Colour getModuleBorderColor() { return colorHelper(juce::Colour(0xff475d9d)); }
    inline juce::Colour getTitleColor() { return colorHelper(juce::Colour(0xff2c92ca)); }
    //inline juce::Colour getAnalyzerGridColor() { return colorHelper(juce::Colour(0xff262626)); } // Базовый цвет сетки - темный
    inline juce::Colour getTickColor() { return colorHelper(juce::Colour(0xff313131)); }
    inline juce::Colour getMeterLineColor() { return colorHelper(juce::Colour(0xff3c3c3c)); }
    //inline juce::Colour getScaleTextColor() { return colorHelper(juce::Colours::snow); } // Используем для текста на шкалах
    inline juce::Colour getComponentOutlineColor() { return colorHelper(juce::Colours::darkgrey); }
    // --- Новые или переопределенные цвета для Analyzer/Overlay ---
    /*inline juce::Colour getAnalyzerBackgroundColor() { return colorHelper(juce::Colours::transparentBlack); }*/
    // Базовый цвет для заливки спектра (альфа применяется при рисовании)
    //inline juce::Colour getSpectrumFillBaseColor() { return getInputSignalColor(); /* Или другой базовый цвет */ }
    // Цвет линии спектра (совпадает с базовым для заливки или цветом входного сигнала)
   /* inline juce::Colour getSpectrumLineColor() { return getInputSignalColor(); }*/
    // Базовый цвет для линии удержания пиков (альфа применяется при рисовании)
    //inline juce::Colour getPeakHoldLineBaseColor() { return colorHelper(juce::Colours::darkblue); }
    // Цвет линии выше 0 дБ
    //inline juce::Colour getOverZeroDbLineColor() { return colorHelper(juce::Colours::red); }
    // Базовый цвет линии 0 дБ (альфа применяется при рисовании)
    /*inline juce::Colour getZeroDbLineBaseColor() { return colorHelper(juce::Colours::white); }*/
    // Базовый цвет сетки анализатора (альфа применяется при рисовании)
    /*inline juce::Colour getAnalyzerGridBaseColor() { return colorHelper(juce::Colours::dimgrey); }*/
    // Цвет текста пикового значения
    //inline juce::Colour getAnalyzerPeakTextColor() { return getScaleTextColor(); /* Или juce::Colours::white */ }
    // Цвет для Mid/High кроссовера
    //inline juce::Colour getMidHighCrossoverColor() { return colorHelper(juce::Colours::cyan); }
    inline juce::Colour BackgroundColor() { return colorHelper(juce::Colours::black); }
} // конец namespace ColorScheme


struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    void drawRotarySlider(juce::Graphics&,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override;

    void drawToggleButton(juce::Graphics& g,
        juce::ToggleButton& toggleButton,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
};

// --- END OF FILE LookAndFeel.h ---
