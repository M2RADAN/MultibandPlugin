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

void LookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle, // Например, ~225 градусов или 7*pi/4
    float rotaryEndAngle,   // Например, ~-45 градусов или -pi/4 (но в диапазоне 0-2pi это будет ~315 или 5*pi/4)
    juce::Slider& slider)
{
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);
    auto center = bounds.getCentre();
    // Оставляем немного места по краям для указателя/меток
    float radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 5.0f;
    float trackThickness = radius * 0.10f; // Толщина трека (например, 25% от радиуса)
    // Уменьшаем радиус на половину толщины трека, чтобы трек рисовался вокруг этого радиуса
    float arcRadius = radius - trackThickness / 2.0f;

    // --- 1. Рисуем фон всего компонента слайдера (если он отличается от общего фона) ---
    g.setColour(ColorScheme::getRotarySliderBackBodyColor()); // Пример фона под слайдером
    g.fillEllipse(bounds); // Если нужен круглый фон для всего компонента

    // --- 2. Рисуем полный неактивный трек (темный) ---
    Path backgroundTrack;
    backgroundTrack.addCentredArc(center.x, center.y,
        arcRadius, arcRadius,
        0.0f,             // Начальный угол для Path
        rotaryStartAngle, // Фактический начальный угол дуги
        rotaryEndAngle,   // Фактический конечный угол дуги
        true);            // true = начать новый sub-path
    g.setColour(ColorScheme::getRotarySliderTrackColor());
    g.strokePath(backgroundTrack, PathStrokeType(trackThickness, PathStrokeType::curved, PathStrokeType::butt)); // butt для ровных краев

    // --- 3. Рисуем дугу активного значения (яркая) ---
    if (sliderPosProportional > 0.0f)
    {
        Path valueArc;
        float valueAngle = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        valueArc.addCentredArc(center.x, center.y,
            arcRadius, arcRadius,
            0.0f,
            rotaryStartAngle,
            valueAngle,
            true);
        g.setColour(ColorScheme::getRotarySliderValueArcColor());
        g.strokePath(valueArc, PathStrokeType(trackThickness, PathStrokeType::curved, PathStrokeType::butt));
    }

    // --- 4. Рисуем центральный круг (маскирует внутреннюю часть дуг) ---
    float centralCircleRadius = arcRadius - trackThickness / 2.0f - 1.0f; // Чуть меньше внутреннего радиуса дуги
    if (centralCircleRadius > 0) {
        g.setColour(ColorScheme::getRotarySliderBodyColor()); // Цвет фона слайдера
        g.fillEllipse(center.x - centralCircleRadius,
            center.y - centralCircleRadius,
            centralCircleRadius * 2.0f,
            centralCircleRadius * 2.0f);
    }

    // --- 5. Рисуем указатель (Thumb) - треугольник ---
    float valueAngleForThumb = jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

    // --- Треугольный указатель (новый вариант) ---
    Path thumbPath;

    // Радиус для острия треугольника: на внешней границе дуги значения
    float tipRadius = arcRadius + trackThickness * 0.5f;
    Point<float> tip = center.getPointOnCircumference(tipRadius, valueAngleForThumb);

    // Радиус для основания треугольника: немного ВНУТРЬ от центральной линии дуги
    // или даже на внутреннюю границу дуги
    float baseRadius = arcRadius - trackThickness * 0.3f; // Попробуйте значения от 0.1f до 0.5f * trackThickness
    // чтобы подобрать "глубину"

// Ширина основания треугольника (определяется углом)
    float halfBaseAngle = degreesToRadians(12.0f); // Угол для получения нужной ширины основания
    // Чем больше угол, тем шире основание

    thumbPath.startNewSubPath(tip); // Начинаем с острия
    thumbPath.lineTo(center.getPointOnCircumference(baseRadius, valueAngleForThumb - halfBaseAngle));
    thumbPath.lineTo(center.getPointOnCircumference(baseRadius, valueAngleForThumb + halfBaseAngle));
    thumbPath.closeSubPath(); // Замыкаем треугольник

    g.setColour(ColorScheme::getRotarySliderThumbColor());
    g.fillPath(thumbPath);


    // --- 6. Рисуем текст значения в центре ---
    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        String textToDisplay = rswl->getDisplayString();
        // Область для текста примерно равна центральному кругу
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
    g.fillRoundedRectangle(trackRect, trackWidth / 2.0f);

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
