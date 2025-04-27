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

    // --- Draw Title ---
    // Use FontOptions
    auto titleFont = juce::Font(juce::FontOptions(static_cast<float>(getTextHeight() + 1), Font::bold)); // Slightly larger/bold
    g.setColour(ColorScheme::getTitleColor());
    g.setFont(titleFont);
    // Draw title centered at the top
    g.drawFittedText(getName(),
        localBounds.removeFromTop(getTextHeight() + 4), // Allocate space for title
        Justification::centredBottom, 1);

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
        textBounds.setX(juce::jmax(0.0f, textBounds.getX())); // Don't go left of 0
        textBounds.setRight(juce::jmin(static_cast<float>(localBounds.getWidth()), textBounds.getRight())); // Don't go right of component width

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
    r.setCentre(bounds.getCentreX(), bounds.getY() + size / 2);

    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    // Handle choice parameters first
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    // Handle float parameters
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float value = static_cast<float>(getValue()); // Use float for calculations
        bool addK = false;

        // Check if suffix is needed and value is large (for frequency etc.)
        if (suffix.isNotEmpty() && suffix.containsIgnoreCase("Hz")) // Example check
            addK = MBRP_GUI::truncateKiloValue(value); // Assuming this helper exists

        // Determine decimal places: 0 if 'k' is added, 1 or 2 otherwise?
        int decimalPlaces = addK ? 1 : 0; // Adjust as needed
        // Special handling for pan?
        if (getName().containsIgnoreCase("Pan")) decimalPlaces = 0; // No decimals for pan usually

        juce::String str = juce::String(value, decimalPlaces);

        if (suffix.isNotEmpty())
        {
            str << " "; // Add space before suffix
            if (addK)
                str << "k";
            str << suffix;
        }
        return str;
    }

    // Fallback for other parameter types (shouldn't happen with current usage)
    jassertfalse;
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