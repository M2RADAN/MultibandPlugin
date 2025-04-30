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
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    using namespace juce;
    using namespace ColorScheme; // Assuming ColorScheme is defined in LookAndFeel.h

    auto bounds = Rectangle<int>(x, y, width, height).toFloat();
    auto enabled = slider.isEnabled();

    // Draw the background ellipse
    g.setColour(enabled ? getSliderFillColor() : Colours::darkgrey);
    g.fillEllipse(bounds);

    // Draw the border ellipse
    g.setColour(enabled ? getSliderBorderColor() : Colours::grey);
    g.drawEllipse(bounds, 2.f); // Use float thickness

    // Draw the pointer and text label if it's a RotarySliderWithLabels
    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        Path pointerPath;

        // Define the pointer shape
        Rectangle<float> pointerRect;
        pointerRect.setLeft(center.getX() - 2.f); // Use float
        pointerRect.setRight(center.getX() + 2.f);
        pointerRect.setTop(bounds.getY());
        // Use static_cast for explicit conversion
        pointerRect.setBottom(juce::jmax(center.getY() - static_cast<float>(rswl->getTextHeight()) * 1.5f,
            bounds.getY() + 15.f)); // Use float

        pointerPath.addRoundedRectangle(pointerRect, 2.f);

        // Calculate the angle based on slider position
        jassert(rotaryStartAngle < rotaryEndAngle);
        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);

        // Rotate and draw the pointer
        pointerPath.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));
        g.fillPath(pointerPath);

        // --- Draw the center text label ---
        // Use FontOptions and TextLayout
        auto labelFont = juce::Font(juce::FontOptions(static_cast<float>(rswl->getTextHeight())));
        g.setFont(labelFont);
        auto text = rswl->getDisplayString();
        float textWidth = getTextLayoutWidth(text, labelFont);
        // ----------------------------------

        // Define bounds for the text
        Rectangle<float> textBounds;
        textBounds.setSize(textWidth + 4.f, // Use float
            static_cast<float>(rswl->getTextHeight() + 2)); // Use float
        textBounds.setCentre(bounds.getCentre());

        // Draw the text
        g.setColour(enabled ? ColorScheme::getTitleColor() : Colours::lightgrey);
        g.drawFittedText(text, textBounds.toNearestInt(), juce::Justification::centred, 1);
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
        const float cornerSize = 4.0f;

        Colour backgroundColour;
        Colour borderColour;
        Colour textColour;

        if (buttonIsOn) {
            // Определяем цвет фона активной кнопки по ее имени (тексту)
            // Это менее надежно, чем если бы кнопки имели ID или если бы LookAndFeel
            // имел доступ к контексту, но для 3 кнопок это сработает.
            if (toggleButton.getButtonText().equalsIgnoreCase("Low")) {
                backgroundColour = ColorScheme::getLowBandColor();
            }
            else if (toggleButton.getButtonText().equalsIgnoreCase("Mid")) {
                backgroundColour = ColorScheme::getMidBandColor();
            }
            else if (toggleButton.getButtonText().equalsIgnoreCase("High")) {
                backgroundColour = ColorScheme::getHighBandColor();
            }
            else {
                backgroundColour = ColorScheme::getSliderThumbColor(); // Запасной цвет
            }
            borderColour = backgroundColour.darker(0.2f); // Граница чуть темнее фона
            textColour = ColorScheme::getToggleButtonOnTextColor(); // Белый текст
        }
        else {
            backgroundColour = ColorScheme::getToggleButtonOffColor();   // Светло-серый фон
            borderColour = ColorScheme::getToggleButtonOffBorder(); // Серая граница
            textColour = ColorScheme::getToggleButtonOffTextColor(); // Темно-серый текст
        }

        // Рисуем фон
        g.setColour(backgroundColour);
        g.fillRoundedRectangle(boundsRect.toFloat(), cornerSize);

        // Рисуем границу
        g.setColour(borderColour);
        g.drawRoundedRectangle(boundsRect.toFloat(), cornerSize, 1.f);

        // Рисуем текст
        g.setColour(textColour);
        g.setFont(juce::Font(14.0f)); // Можно сделать шрифт чуть жирнее: Font(14.0f, Font::bold)
        g.drawFittedText(toggleButton.getButtonText(), boundsRect, Justification::centred, 1);
    }
}
