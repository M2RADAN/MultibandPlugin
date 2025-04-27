#include "CustomButtons.h"

void AnalyzerButton::resized()
{
    auto bounds = getLocalBounds();
    auto insetRect = bounds.reduced(4); // Reduce bounds for drawing inside

    randomPath.clear(); // Clear previous path

    juce::Random r; // Random number generator

    // Start the random path
    // Use static_cast for explicit type conversions from int to float
    randomPath.startNewSubPath(static_cast<float>(insetRect.getX()),
        static_cast<float>(insetRect.getY()) + static_cast<float>(insetRect.getHeight()) * r.nextFloat());

    // Add line segments to the path with random Y values
    // Iterate using float to avoid repeated casting inside the loop
    for (float x = static_cast<float>(insetRect.getX() + 1);
        x < static_cast<float>(insetRect.getRight());
        x += 2.0f) // Increment by 2 pixels
    {
        randomPath.lineTo(x,
            static_cast<float>(insetRect.getY()) + static_cast<float>(insetRect.getHeight()) * r.nextFloat());
    }
}