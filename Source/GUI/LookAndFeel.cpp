#include "LookAndFeel.h"
#include "RotarySliderWithLabels.h"
#include "CustomButtons.h"
#include <juce_gui_basics/juce_gui_basics.h> // Include for TextLayout etc.

// --- Helper function to get text width using TextLayout ---
static float getTextLayoutWidth(const juce::String& text, const juce::Font& font)
{
    juce::TextLayout textLayout;
    juce::AttributedString attrString(text);
    attrString.setFont(font);
    // Create layout with a large width to get the natural width
    textLayout.createLayout(attrString, 10000.0f);
    return textLayout.getWidth();
}
// ----------------------------------------------------

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton& button, int /*buttonHeight*/)
{
    juce::String id = button.getComponentID();
    if (id == "OverlaySoloButton" || id == "OverlayMuteButton" || id == "OverlayBypassButton")
    {
        return juce::Font(10.0f, juce::Font::bold); // Маленький жирный шрифт для кнопок на оверлее
    }
    // ... (можно добавить другие проверки для других кнопок)

    // Шрифт по умолчанию для остальных TextButton
    return juce::Font(14.0f);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional, // Это значение от 0.0 (минимум) до 1.0 (максимум)
    float rotaryStartAngleOriginal, // Оригинальный начальный угол для всего диапазона слайдера
    float rotaryEndAngleOriginal,   // Оригинальный конечный угол для всего диапазона слайдера
    juce::Slider& slider)
{
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);
    auto center = bounds.getCentre();
    float radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 5.0f;
    float trackThickness = radius * 0.10f;
    float arcRadius = radius - trackThickness / 2.0f;

    // --- 1. Фон (без изменений) ---
    g.setColour(ColorScheme::getRotarySliderBackBodyColor());
    g.fillEllipse(bounds);

    // --- 2. Полный неактивный трек (без изменений) ---
    Path backgroundTrack;
    backgroundTrack.addCentredArc(center.x, center.y,
        arcRadius, arcRadius,
        0.0f,
        rotaryStartAngleOriginal,
        rotaryEndAngleOriginal,
        true);
    g.setColour(ColorScheme::getRotarySliderTrackColor());
    g.strokePath(backgroundTrack, PathStrokeType(trackThickness, PathStrokeType::curved, PathStrokeType::butt));

    // --- 3. Рисуем дугу активного значения (яркая) ---
    // Определяем, является ли это Pan-слайдером
    bool isPanSlider = slider.getName().equalsIgnoreCase("Pan");
    // Или, если вы передаете параметр и можете его проверить:
    // if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider)) {
    //     if (rswl->getParameter() != nullptr && rswl->getParameter()->getName(100).containsIgnoreCase("Pan")) {
    //         isPanSlider = true;
    //     }
    // }


    if (sliderPosProportional > 0.0f || isPanSlider) // Для Pan рисуем всегда, даже если значение в центре
    {
        Path valueArc;
        float valueAngle = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngleOriginal, rotaryEndAngleOriginal);
        float startAngleForValueArc = rotaryStartAngleOriginal;

        if (isPanSlider)
        {
            // Для Pan-слайдера:
            // rotaryStartAngleOriginal соответствует -1 (L)
            // rotaryEndAngleOriginal соответствует +1 (R)
            // Центральное положение (C) соответствует sliderPosProportional = 0.5
            // Угол для центра (C) - это середина между rotaryStartAngleOriginal и rotaryEndAngleOriginal
            float centerAngle = (rotaryStartAngleOriginal + rotaryEndAngleOriginal) / 2.0f;

            // Если диапазон пересекает 0/2PI (например, от 7*pi/4 до 9*pi/4 (эквивалент pi/4)),
            // нужно быть осторожным с вычислением centerAngle.
            // Стандартные углы JUCE для Rotary (180+45 до 180-45 + 2PI) обычно не создают этой проблемы,
            // так как они идут, например, от ~225 градусов до ~495 градусов (или 135 по второму кругу).
            // Центр будет (225+495)/2 = 720/2 = 360 (или 0). Это верхняя точка.
            // Однако, если rotaryEndAngleOriginal < rotaryStartAngleOriginal (из-за добавления 2PI),
            // то (rotaryStartAngleOriginal + (rotaryEndAngleOriginal - MathConstants<float>::twoPi)) / 2.0f;
            // или проще: jmap(0.5f, 0.0f, 1.0f, rotaryStartAngleOriginal, rotaryEndAngleOriginal)
            centerAngle = jmap(0.5f, 0.0f, 1.0f, rotaryStartAngleOriginal, rotaryEndAngleOriginal);


            if (sliderPosProportional == 0.5f) // Точно в центре
            {
                // Не рисуем дугу, или рисуем очень маленькую точку в центре
                // Для простоты пока не будем рисовать дугу, если значение точно "C"
                // LookAndFeel для текста "C" внутри слайдера позаботится об индикации
            }
            else if (sliderPosProportional < 0.5f) // Значение слева от центра (L)
            {
                // Рисуем от valueAngle (текущее положение слева) до centerAngle (C)
                // Порядок важен для addCentredArc: от меньшего угла к большему (по часовой стрелке)
                // или наоборот, если startNewSubPath=false и мы продолжаем путь
                // Для addCentredArc, если startAngle > endAngle, он пойдет против часовой.
                // Нам нужно от центра к значению.
                startAngleForValueArc = valueAngle; // Текущее значение (например, L)
                valueAngle = centerAngle;          // Конечный угол (C)
                // Убедимся, что startAngleForValueArc < valueAngle для корректной отрисовки по часовой
                if (startAngleForValueArc > valueAngle) std::swap(startAngleForValueArc, valueAngle);

            }
            else // sliderPosProportional > 0.5f, значение справа от центра (R)
            {
                // Рисуем от centerAngle (C) до valueAngle (текущее положение справа)
                startAngleForValueArc = centerAngle; // Начальный угол (C)
                // valueAngle уже содержит угол для текущего значения справа
            }
        }

        // Рисуем дугу, только если начальный и конечный углы не совпадают (для Pan в центре)
        if (!isPanSlider || sliderPosProportional != 0.5f) {
            valueArc.addCentredArc(center.x, center.y,
                arcRadius, arcRadius,
                0.0f,                     // Угол начала пути (не самой дуги)
                startAngleForValueArc,    // Фактический начальный угол дуги
                valueAngle,               // Фактический конечный угол дуги
                true);                    // Начать новый sub-path
            g.setColour(slider.findColour(Slider::rotarySliderFillColourId)); // Используем цвет заливки из слайдера
            g.strokePath(valueArc, PathStrokeType(trackThickness, PathStrokeType::curved, PathStrokeType::butt));
        }
    }

    // --- 4. Центральный круг (без изменений) ---
    float centralCircleRadius = arcRadius - trackThickness / 2.0f - 1.0f;
    if (centralCircleRadius > 0) {
        g.setColour(ColorScheme::getRotarySliderBodyColor());
        g.fillEllipse(center.x - centralCircleRadius,
            center.y - centralCircleRadius,
            centralCircleRadius * 2.0f,
            centralCircleRadius * 2.0f);
    }

    // --- 5. Указатель (Thumb) - треугольник (без изменений) ---
    // Указатель всегда показывает на текущее значение valueAngle (которое мы рассчитали из sliderPosProportional)
    float angleForThumb = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngleOriginal, rotaryEndAngleOriginal);
    Path thumbPath;
    float tipRadius = arcRadius + trackThickness * 0.5f;
    Point<float> tip = center.getPointOnCircumference(tipRadius, angleForThumb);
    float baseRadius = arcRadius - trackThickness * 0.3f;
    float halfBaseAngle = degreesToRadians(12.0f);
    thumbPath.startNewSubPath(tip);
    thumbPath.lineTo(center.getPointOnCircumference(baseRadius, angleForThumb - halfBaseAngle));
    thumbPath.lineTo(center.getPointOnCircumference(baseRadius, angleForThumb + halfBaseAngle));
    thumbPath.closeSubPath();
    g.setColour(slider.findColour(Slider::thumbColourId)); // Используем цвет ручки из слайдера
    g.fillPath(thumbPath);

    // --- 6. Текст значения в центре (без изменений) ---
    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        String textToDisplay = rswl->getDisplayString();
        float textRectSide = centralCircleRadius > 0 ? centralCircleRadius * 1.6f : radius * 0.7f;
        Rectangle<float> textBounds = Rectangle<float>(textRectSide, textRectSide * 0.6f).withCentre(center);
        g.setColour(ColorScheme::getRotarySliderTextColor());
        float fontSize = jmin(radius * 0.35f, textBounds.getHeight() * 0.7f);
        if (textToDisplay.length() <= 2) fontSize = jmin(radius * 0.45f, textBounds.getHeight() * 0.8f);
        g.setFont(Font(fontSize, Font::bold));
        g.drawText(textToDisplay, textBounds.toNearestInt(), Justification::centred, false);
    }
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // Mark unused parameters
    juce::ignoreUnused(minSliderPos, maxSliderPos, style);

    using namespace juce;
    auto bounds = Rectangle<int>(x, y, width, height).toFloat();
    auto trackWidth = 4.0f;

    // Draw track
    auto trackRect = bounds.withSizeKeepingCentre(bounds.getWidth(), trackWidth);
    g.setColour(slider.findColour(Slider::trackColourId)); // Use defined track color
    g.fillRoundedRectangle(trackRect, trackWidth / 1.0f);

    // Draw thumb
    auto thumbWidth = 10.0f;
    // sliderPos is already the pixel position of the thumb center (usually)
    auto thumbRect = Rectangle<float>(thumbWidth, bounds.getHeight())
        .withCentre({ sliderPos, bounds.getCentreY() }); // Use Point<float>

    g.setColour(slider.findColour(Slider::thumbColourId)); // Use defined thumb color
    g.fillRoundedRectangle(thumbRect, 3.0f);
    g.setColour(slider.findColour(Slider::thumbColourId).darker());
    g.drawRoundedRectangle(thumbRect, 3.0f, 1.0f); // Draw border
}


void LookAndFeel::drawToggleButton(juce::Graphics& g,
    juce::ToggleButton& toggleButton,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    // Mark unused parameters
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    using namespace juce;

    // Handle specific button types first
    if (auto* pb = dynamic_cast<PowerButton*>(&toggleButton))
    {
        Path powerButtonPath;
        auto bounds = toggleButton.getLocalBounds();
        auto size = static_cast<float>(jmin(bounds.getWidth(), bounds.getHeight()) - 6);
        auto r = bounds.withSizeKeepingCentre(roundToInt(size), roundToInt(size)).toFloat();
        float ang = 30.f;
        size -= 6.f;

        powerButtonPath.addCentredArc(r.getCentreX(), r.getCentreY(), size * 0.5f, size * 0.5f, 0.f, degreesToRadians(ang), degreesToRadians(360.f - ang), true);
        powerButtonPath.startNewSubPath(r.getCentreX(), r.getY());
        powerButtonPath.lineTo(r.getCentre());

        PathStrokeType strokeType(2.f, PathStrokeType::JointStyle::curved);

        // --- Меняем цвет в зависимости от состояния ---
        // Используем яркий цвет (например, thumb color) для "выключено" (не bypass)
        // Используем тусклый/серый цвет для "включено" (bypass)
        auto color = !pb->getToggleState() ? ColorScheme::getSliderThumbColor() // Яркий, когда обработка ВКЛючена (Bypass ВЫКлючен)
            : ColorScheme::getSecondaryTextColor(); // Тусклый, когда обработка ВЫКЛючена (Bypass ВКЛючен)
        // ----------------------------------------------
        g.setColour(color);
        g.strokePath(powerButtonPath, strokeType);
    }
    else if (auto* analyzerButton = dynamic_cast<AnalyzerButton*>(&toggleButton))
    {
        // Determine color based on state
        auto color = !toggleButton.getToggleState() ? Colours::dimgrey : Colour(0u, 172u, 1u);
        g.setColour(color);

        auto bounds = toggleButton.getLocalBounds();
        // Draw a simple rectangle border for this button
        g.drawRect(bounds);
        // Draw the random path stored within the button
        g.strokePath(analyzerButton->randomPath, PathStrokeType(1.f));
    }
    else // Default ToggleButton style (used for BandSelectControls)
    {
        auto boundsRect = toggleButton.getLocalBounds().reduced(2);
        auto buttonIsOn = toggleButton.getToggleState();
        const float cornerSize = 3.0f;

        Colour backgroundColour;
        Colour borderColour;
        Colour textColour;

        juce::String buttonText = toggleButton.getButtonText();

        if (buttonIsOn) {
            if (buttonText.equalsIgnoreCase("Low")) {
                backgroundColour = ColorScheme::getLowBandColor();
            }
            else if (buttonText.equalsIgnoreCase("L-Mid")) {
                backgroundColour = ColorScheme::getLowMidBandColor(); // Новый цвет
            }
            else if (buttonText.equalsIgnoreCase("M-High")) {
                backgroundColour = ColorScheme::getMidHighBandColor(); // Старый Mid
            }
            else if (buttonText.equalsIgnoreCase("High")) {
                backgroundColour = ColorScheme::getHighBandAltColor(); // Старый High
            }
            else {
                backgroundColour = ColorScheme::getSliderThumbColor(); // Запасной
            }
            borderColour = backgroundColour.darker(0.3f);
            textColour = ColorScheme::getToggleButtonOnTextColor();
        }
        else {
            backgroundColour = ColorScheme::getToggleButtonOffColor();
            borderColour = ColorScheme::getToggleButtonOffBorder();
            textColour = ColorScheme::getToggleButtonOffTextColor();
        }


        // Рисуем фон
        g.setColour(backgroundColour);
        g.fillRoundedRectangle(boundsRect.toFloat(), cornerSize);

        g.setColour(borderColour);
        g.drawRoundedRectangle(boundsRect.toFloat(), cornerSize, 1.5f); // Чуть толще граница

        g.setColour(textColour);
        // Подбираем размер шрифта
        float fontSize = juce::jmin(14.0f, boundsRect.getHeight() * 0.6f);
        if (buttonText.length() > 4) fontSize *= 0.85f; // Уменьшаем для длинных текстов типа "M-High"

        g.setFont(juce::Font(fontSize, Font::bold));
        g.drawFittedText(buttonText, boundsRect, Justification::centred, 1);
    }
}
