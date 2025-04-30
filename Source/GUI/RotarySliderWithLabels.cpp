#include "RotarySliderWithLabels.h"
#include "Utilities.h"       // Assuming getValString, truncateKiloValue are here
#include "LookAndFeel.h"     // For ColorScheme
#include <juce_gui_basics/juce_gui_basics.h> // For TextLayout

// --- Helper function to get text width using TextLayout ---
// (Can be moved to Utilities.h if used elsewhere)
static float getTextLayoutWidth(const juce::String& text, const juce::Font& font)
{
    juce::TextLayout textLayout;
    juce::AttributedString attrString(text);
    attrString.setFont(font);
    textLayout.createLayout(attrString, 10000.0f); // Large width to get natural size
    return textLayout.getWidth();
}
// ----------------------------------------------------

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;

    auto startAng = degreesToRadians(180.f + 45.f); // Start angle for the rotary arc
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi; // End angle
    auto range = getRange();
    auto sliderBounds = getSliderBounds(); // Get bounds for the rotary part
    auto localBounds = getLocalBounds();   // Get total component bounds

    //// --- Draw Title ---
    //// Use FontOptions
    //auto titleFont = juce::Font(juce::FontOptions(static_cast<float>(getTextHeight() + 1), Font::bold)); // Slightly larger/bold
    //g.setColour(ColorScheme::getTitleColor());
    //g.setFont(titleFont);
    //// Draw title centered at the top
    //g.drawFittedText(getName(),
    //    localBounds.removeFromTop(getTextHeight() + 4), // Allocate space for title
    //    Justification::centredBottom, 1);

    // --- Delegate rotary drawing to LookAndFeel ---
    getLookAndFeel().drawRotarySlider(g,
        sliderBounds.getX(),
        sliderBounds.getY(),
        sliderBounds.getWidth(),
        sliderBounds.getHeight(),
        // Map value to proportional position [0, 1]
        (float)jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng,
        endAng,
        *this);

    // --- Draw Min/Max/Custom Labels around the slider ---
    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f; // Radius of the slider bounds

    g.setColour(ColorScheme::getSliderRangeTextColor());
    // Use FontOptions
    auto labelFont = juce::Font(juce::FontOptions(static_cast<float>(getTextHeight())));
    g.setFont(labelFont);

    // Iterate through defined labels
    for (const auto& labelPos : labels)
    {
        auto pos = labelPos.pos; // Normalized position (0.0 to 1.0)
        jassert(0.f <= pos && pos <= 1.f); // Ensure valid position

        // Map normalized position to angle
        auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);

        // Calculate label position on circumference outside the slider
        // Use static_cast for explicit conversions
        auto labelCenter = center.getPointOnCircumference(radius + static_cast<float>(getTextHeight()) * 0.7f, ang); // Adjust distance

        // --- Calculate text bounds ---
        auto str = labelPos.label;
        float textWidth = getTextLayoutWidth(str, labelFont); // Use TextLayout
        Rectangle<float> textBounds;
        textBounds.setSize(textWidth, static_cast<float>(getTextHeight()));
        textBounds.setCentre(labelCenter);

        // --- Adjust bounds to prevent going off-screen ---
        // Simple horizontal check:
        //textBounds.setX(juce::jmax(0.0f, textBounds.getX())); // Don't go left of 0
        //textBounds.setRight(juce::jmin(static_cast<float>(localBounds.getWidth()), textBounds.getRight())); // Don't go right of component width

        // Draw the label text
        g.drawFittedText(str, textBounds.toNearestInt(), Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    // Remove space for the title at the top
    // Use static_cast for conversion
    bounds.removeFromTop(getTextHeight() + 4); // Match space removed in paint

    // Calculate the largest square that fits, leaving space for labels below
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    // Use static_cast for conversion
    size -= static_cast<int>(static_cast<float>(getTextHeight()) * 1.5f); // Reduce size to leave space for labels
    size = juce::jmax(0, size); // Ensure size is not negative

    juce::Rectangle<int> r;
    r.setSize(size, size);
    // Center the square within the remaining bounds horizontally, place it at the top vertically
    r.setCentre(bounds.getCentre()); // Центрируем по вертикали и горизонтали

    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    // --- Специальная обработка для Pan ---
    // Проверяем по имени параметра или по суффиксу, если он уникален для Pan
    bool isPanSlider = (param != nullptr && param->getName(100).containsIgnoreCase("Pan"));
    // Или можно проверить по ID, если они известны:
    // bool isPanSlider = (param != nullptr && (param->paramID == "lowPan" || param->paramID == "midPan" || param->paramID == "highPan"));

    if (isPanSlider)
    {
        float value = static_cast<float>(getValue()); // Текущее значение от -1.0 до 1.0

        // Проверяем на центр с небольшим допуском
        if (std::abs(value) < 0.01f) {
            return "C"; // Центр
        }
        else {
            // Вычисляем процент и направление
            int percentage = static_cast<int>(std::round(std::abs(value) * 100.0f));
            if (value < 0.0f) {
                return "L" + juce::String(percentage); // Лево
            }
            else {
                return "R" + juce::String(percentage); // Право
            }
        }
    }
    // --- Конец специальной обработки для Pan ---


    // --- Стандартная обработка для других типов параметров ---
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float value = static_cast<float>(getValue());
        bool addK = false;

        // Используем оригинальную логику MBRP_GUI::truncateKiloValue
        if (suffix.isNotEmpty() && suffix.containsIgnoreCase("Hz") && value >= 1000.0f) {
            addK = MBRP_GUI::truncateKiloValue(value); // Эта функция должна делить value на 1000, если оно >= 1000
        }

        // Определяем количество знаков после запятой
        int decimalPlaces = 0;
        if (!addK && suffix.containsIgnoreCase("Hz")) {
            // Без "k" для Hz - обычно целые числа
            decimalPlaces = 0;
        }
        else if (addK) {
            // С "k" - один знак после запятой
            decimalPlaces = 1;
        }
        // Добавьте другие условия для других суффиксов, если нужно

        juce::String str = juce::String(value, decimalPlaces);

        if (suffix.isNotEmpty())
        {
            str << " ";
            if (addK)
                str << "k";
            str << suffix;
        }
        return str;
    }

    // Fallback
    jassertfalse; // Не должно происходить для известных типов
    return juce::String(getValue());
}

void RotarySliderWithLabels::changeParam(juce::RangedAudioParameter* p)
{
    // Update the internal parameter pointer and repaint
    param = p;
    repaint();
}
//==============================================================================
// Implementation for RatioSlider (if used)
juce::String RatioSlider::getDisplayString() const
{
    auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param);
    jassert(choiceParam != nullptr); // Ensure it's a choice parameter

    auto currentChoice = choiceParam->getCurrentChoiceName();
    // Remove ".0" if present (e.g., "4.0" -> "4")
    if (currentChoice.contains(".0"))
        currentChoice = currentChoice.upToFirstOccurrenceOf(".", false, false);

    // Append ":1" for ratio display
    currentChoice << ":1";

    return currentChoice;
}