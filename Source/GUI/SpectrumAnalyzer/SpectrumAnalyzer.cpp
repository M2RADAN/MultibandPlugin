#include "SpectrumAnalyzer.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <juce_gui_basics/juce_gui_basics.h> // For TextLayout

// --- Helper function to get text width using TextLayout ---
static float getTextLayoutWidth(const juce::String& text, const juce::Font& font)
{
    juce::TextLayout textLayout;
    juce::AttributedString attrString(text);
    attrString.setFont(font);
    textLayout.createLayout(attrString, 10000.0f); // Large width to get natural size
    return textLayout.getWidth();
}
// ----------------------------------------------


//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& p) :
    audioProcessor(p),
    // Initialize vector size based on processor's fftSize constant
    spectrumData(size_t(MBRPAudioProcessor::fftSize / 2)) // Use size_t cast
{
    peakDbLevel.store(minDb); // Initialize atomic peak level
    // Fill initial spectrum data with minimum dB value
    std::fill(spectrumData.begin(), spectrumData.end(), minDb);
    // Start the timer for periodic updates
    startTimerHz(30); // Update rate (e.g., 30 times per second)
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer(); // Stop the timer when the component is destroyed
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    using namespace juce;

    // Get drawing area, slightly reduced from the component bounds
    auto bounds = getLocalBounds().toFloat().reduced(5.f);

    // Draw background, grid lines, and labels first
    drawGrid(g, bounds);
    // Draw the spectrum graph itself
    drawSpectrum(g, bounds);
    // Draw the frequency markers for the EQ/crossover bands on top
    drawFrequencyMarkers(g, bounds);

    // --- Draw Peak Level Readout ---
    g.setColour(textColour); // Use the defined text colour
    // Use FontOptions
    auto peakFont = juce::Font(juce::FontOptions(12.0f));
    g.setFont(peakFont);
    // -----------------------------

    // Atomically load the current peak value
    float currentPeak = peakDbLevel.load();
    // Format the text, showing "-inf" if below minDb
    String peakText = "Peak: " + ((currentPeak <= minDb + 0.01f) // Add tolerance for float comparison
        ? String("-inf dB") // Indicate units even for -inf
        : String(currentPeak, 1) + " dB"); // 1 decimal place

    // Define area for the peak text in the top-right corner relative to bounds
    float peakTextAreaWidth = getTextLayoutWidth(peakText, peakFont) + 10.f; // Calculate width + padding
    float peakTextAreaHeight = 15.f;
    // Position using bounds coordinates
    juce::Rectangle<float> peakTextArea(bounds.getRight() - peakTextAreaWidth,
        bounds.getY(),
        peakTextAreaWidth,
        peakTextAreaHeight);

    // Draw the text for the peak level readout
    g.drawText(peakText,
        peakTextArea.toNearestInt(), // Convert to int rect for drawing
        Justification::centredRight, // Align text to the right
        false);                      // Don't use ellipsis (...)
    // ------------------------------------------
}

void SpectrumAnalyzer::resized()
{
    // Calculations dependent on size could be performed here if needed.
    // For this implementation, calculations are done in paint().
}

void SpectrumAnalyzer::timerCallback()
{
    bool expected = true; // For compare_exchange_strong
    // Check if new FFT data is ready from the processor
    if (audioProcessor.getIsFftDataReady().compare_exchange_strong(expected, false))
    {
        // Get a reference to the FFT magnitude data from the processor
        auto& fftInputDataRef = audioProcessor.getFftData();
        auto numBins = size_t(MBRPAudioProcessor::fftSize / 2); // Use size_t

        // Ensure our internal data vector has the correct size
        if (spectrumData.size() != numBins)
            spectrumData.resize(numBins);

        // --- Normalization Factor ---
        // Factor to scale FFT magnitudes approximately to amplitude (where 1.0 ~ sine at 0dBFS)
        // 2.0 / fftSize is a common starting point. Adjust experimentally if needed.
        const float normalizationFactor = 2.0f / static_cast<float>(MBRPAudioProcessor::fftSize);
        // -----------------------------

        float currentFramePeak = minDb; // Reset peak for the current frame

        // Process each frequency bin
        for (size_t i = 0; i < numBins; ++i)
        {
            // Get magnitude from processor's data
            float magnitude = fftInputDataRef[i];
            // Apply normalization
            float normalizedMagnitude = magnitude * normalizationFactor;

            // Convert normalized magnitude to dB, clamping at minDb
            spectrumData[i] = juce::Decibels::gainToDecibels(normalizedMagnitude, minDb);

            // Update the peak level for this frame if the current bin is higher
            if (spectrumData[i] > currentFramePeak)
            {
                currentFramePeak = spectrumData[i];
            }
        }
        // Atomically store the peak dB level found in this frame
        peakDbLevel.store(currentFramePeak);

        // Request a repaint to display the updated spectrum
        repaint();
    }
    // Optional: Implement peak hold decay logic here if desired
    // Example: if (peakDbLevel.load() > minDb) peakDbLevel.store(peakDbLevel.load() - decayRate);
}

// --- Drawing Helper Functions ---

void SpectrumAnalyzer::drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    using namespace juce;

    // Fill background
    g.fillAll(Colours::black);
    // Draw outer border (using original integer bounds might look sharper)
    g.setColour(Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1.f); // Draw border around the whole component

    // Get drawing area coordinates from the passed float bounds
    auto width = bounds.getWidth();
    auto top = bounds.getY();
    auto bottom = bounds.getBottom();
    auto left = bounds.getX();
    auto right = bounds.getRight();

    // --- Frequency Grid (Vertical Lines) ---
    float freqs[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100,
                      200, 300, 400, 500, 600, 700, 800, 900, 1000,
                      2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
                      20000 };
    g.setColour(gridLineColour);
    for (float f : freqs)
    {
        float x = left + frequencyToX(f, width);
        // Draw line only if it's within the horizontal bounds
        if (x >= left && x <= right)
            g.drawVerticalLine(roundToInt(x), top, bottom); // Use roundToInt for pixel line
    }

    // --- Frequency Labels ---
    float labelFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    g.setColour(textColour);
    // Use FontOptions
    auto freqLabelFont = juce::Font(juce::FontOptions(10.f));
    g.setFont(freqLabelFont);
    // -----------------------------
    for (float f : labelFreqs)
    {
        float x = left + frequencyToX(f, width);
        // Format frequency string (use "k" for kHz)
        String str = (f >= 1000.f) ? String(f / 1000.f, (f < 10000.f ? 1 : 0)) + "k" // More precise kHz formatting
            : String(roundToInt(f));
        // Use TextLayout to get width
        float textW = getTextLayoutWidth(str, freqLabelFont);
        // ----------------------------
        Rectangle<float> r(0, 0, textW, 10.f);
        r.setCentre(x, bottom + 7.f); // Position below the grid
        // Check horizontal bounds before drawing
        if (r.getX() >= left - 2.f && r.getRight() <= right + 2.f) // Allow slight overlap
            g.drawFittedText(str, r.toNearestInt(), Justification::centred, 1);
    }

    // --- Level Grid (Horizontal Lines) & Labels ---
    // Define dB levels for grid lines
    float dBs[] = { 30, 24, 18, 12, 6, 0, -6, -12, -18, -24, -30, -36, -42, -48, -54, -60, -72, -84, -96 }; // More steps
    // Use FontOptions
    auto dbGridLabelFont = juce::Font(juce::FontOptions(9.f));
    g.setFont(dbGridLabelFont);
    // -----------------------------

    for (float db : dBs)
    {
        // Draw only if the level is within the displayable dB range
        if (db >= minDb && db <= maxDb)
        {
            // Map dB value to Y coordinate
            float y = jmap(db, minDb, maxDb, bottom, top);

            // --- Draw Grid Line ---
            // Make 0dB line slightly brighter/different color
            g.setColour((std::abs(db) < 0.01f) ? zeroDbLineColour : gridLineColour);
            g.drawHorizontalLine(roundToInt(y), left, right); // Use roundToInt

            // --- Draw Label next to the Line (Right Side - subtle) ---
            g.setColour(textColour.withAlpha(0.7f)); // Semi-transparent label
            String str = String(roundToInt(db)); // Use roundToInt for label
            if (db > 0.01f && !str.startsWithChar('+')) str = "+" + str; // Add "+" sign for positive values
            // Use TextLayout to get width
            float textW = getTextLayoutWidth(str, dbGridLabelFont);
            // ----------------------------
            // Define rectangle for the text label near the right edge
            Rectangle<int> textBounds(roundToInt(right - textW - 4.f), // X position
                roundToInt(y - 5.f),           // Y position (centered on line)
                roundToInt(textW), 10);        // Width, Height
            // Draw the text if it fits vertically within the grid area
            if (textBounds.getBottom() < bottom + 5 && textBounds.getY() > top - 5) // Allow slight overlap
                g.drawText(str, textBounds, Justification::centredRight);
        }
    }

    // --- Level Labels (Left Side - Major Ticks) ---
    g.setColour(textColour); // Reset color for major labels
    // Use FontOptions
    auto dbMajorLabelFont = juce::Font(juce::FontOptions(10.f));
    g.setFont(dbMajorLabelFont);
    // -----------------------------
    // Show labels only for specific major ticks (e.g., min, max, 0, multiples of 12)
    for (float db : dBs)
    {
        bool isMajorTick = (db == maxDb || db == minDb || std::abs(db) < 0.01f || (juce::roundToInt(db) != 0 && juce::roundToInt(db) % 12 == 0));
        if (db >= minDb && db <= maxDb && isMajorTick)
        {
            float y = jmap(db, minDb, maxDb, bottom, top);
            String str = String(roundToInt(db)); // Use roundToInt
            if (db > 0.01f && !str.startsWithChar('+')) str = "+" + str;
            // Use TextLayout to get width
            float textW = getTextLayoutWidth(str, dbMajorLabelFont);
            // ----------------------------
            Rectangle<float> r(0, 0, textW, 10.f);
            r.setCentre(left - (textW / 2.f) - 4.f, y); // Position left of the grid, centered vertically
            // Check vertical bounds before drawing
            if (r.getY() >= top - 5.f && r.getBottom() <= bottom + 5.f) // Allow slight overlap
                g.drawFittedText(str, r.toNearestInt(), Justification::centredRight, 1);
        }
    }
}


void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    using namespace juce;

    auto width = bounds.getWidth();
    auto top = bounds.getY();
    auto bottom = bounds.getBottom();
    auto left = bounds.getX();
    auto numBins = spectrumData.size();
    auto sampleRate = audioProcessor.getSampleRate(); // Get current sample rate

    if (numBins == 0 || sampleRate <= 0) return; // Avoid drawing if no data or invalid state

    // --- Draw main spectrum line (below 0dB or all if no clipping visualization) ---
    Path spectrumPath;
    // Start path from the bottom-left edge corresponding to minFreq
    spectrumPath.startNewSubPath(left + frequencyToX(minFreq, width), bottom);

    for (size_t i = 1; i < numBins; ++i) // Start from bin 1 (skip DC)
    {
        // Calculate frequency for the current bin
        float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);
        // Skip bins outside the displayable frequency range
        if (freq < minFreq || freq > maxFreq) continue;

        float x = left + frequencyToX(freq, width);
        // Map dB value to Y coordinate, clamping Y within the bounds [top, bottom]
        float y = juce::jmap(spectrumData[i], minDb, maxDb, bottom, top);
        y = juce::jlimit(top, y, bottom); // Ensure Y stays within bounds
        spectrumPath.lineTo(x, y);
    }
    // Add final point at maxFreq on the bottom line to close the shape visually
    spectrumPath.lineTo(left + frequencyToX(maxFreq, width), bottom);

    // --- Fill the area below the spectrum line ---
    // Create a gradient fill (optional, looks nice)
    ColourGradient fillGradient(spectrumLineColour.withAlpha(0.4f), left, bottom,
        spectrumLineColour.darker(0.8f).withAlpha(0.1f), left, top, false);
    g.setGradientFill(fillGradient);
    g.fillPath(spectrumPath);

    // --- Draw the spectrum outline ---
    g.setColour(spectrumLineColour); // Use defined spectrum color
    g.strokePath(spectrumPath, PathStrokeType(1.5f)); // Draw the line itself

    // --- Draw spectrum segment above 0dB (clipping indicator) ---
    Path overZeroPath;
    bool pathStarted = false; // Track if the red path segment is active
    float zeroDbY = jmap(0.0f, minDb, maxDb, bottom, top); // Y coordinate of 0dB line
    zeroDbY = juce::jlimit(top, zeroDbY, bottom); // Clamp 0dB line within bounds

    for (size_t i = 1; i < numBins; ++i)
    {
        float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);
        if (freq < minFreq || freq > maxFreq) continue;

        float currentDb = spectrumData[i];
        float x = left + frequencyToX(freq, width);
        // Y coordinate clamped within bounds [top, bottom]
        float y = jmap(currentDb, minDb, maxDb, bottom, top);
        y = juce::jlimit(top, y, bottom);

        // If current point is above (or exactly at) the 0dB line
        if (y <= zeroDbY)
        {
            if (!pathStarted) // Starting a new segment above 0dB
            {
                // Find the previous point to start the segment correctly
                float prevX = (i > 1) ? left + frequencyToX(static_cast<float>(i - 1) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize), width)
                    : x; // Use current x if it's the first relevant bin
                float prevDb = (i > 1) ? spectrumData[i - 1] : minDb;
                float prevY = jmap(prevDb, minDb, maxDb, bottom, top);
                prevY = juce::jlimit(top, prevY, bottom);

                // If the previous point was below 0dB, start the red path from the 0dB line
                if (prevY > zeroDbY)
                {
                    overZeroPath.startNewSubPath(prevX, zeroDbY); // Start at intersection with 0dB
                    overZeroPath.lineTo(x, y);            // Line to the current point (above 0dB)
                }
                else // If the previous point was also above or at 0dB, start from there
                {
                    overZeroPath.startNewSubPath(prevX, prevY);
                    overZeroPath.lineTo(x, y);
                }
                pathStarted = true;
            }
            else // Continue an existing segment above 0dB
            {
                overZeroPath.lineTo(x, y);
            }
        }
        else // Current point is below 0dB
        {
            if (pathStarted) // Just finished a segment above 0dB
            {
                // End the path segment at the 0dB line for the current x
                overZeroPath.lineTo(x, zeroDbY);
                // Draw the completed red segment
                g.setColour(overZeroDbColour); // Use defined clipping color
                g.strokePath(overZeroPath, PathStrokeType(1.5f));
                overZeroPath.clear(); // Clear path for the next potential segment
                pathStarted = false; // Reset flag
            }
            // If already below 0dB, do nothing for the red path
        }
    }
    // If the loop finished while a red segment was active, draw the final part
    if (pathStarted)
    {
        // Optionally, end the path at the right edge on the 0dB line
        // overZeroPath.lineTo(right, zeroDbY);
        g.setColour(overZeroDbColour);
        g.strokePath(overZeroPath, PathStrokeType(1.5f));
    }
}


void SpectrumAnalyzer::drawFrequencyMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    using namespace juce;
    auto width = bounds.getWidth();
    auto top = bounds.getY();
    auto bottom = bounds.getBottom();
    auto left = bounds.getX();
    auto right = bounds.getRight();

    // Get crossover frequencies directly from processor parameters (check for nullptrs)
    float lowFreq = audioProcessor.lowMidCrossover ? audioProcessor.lowMidCrossover->get() : 20.0f; // Default if null
    float highFreq = audioProcessor.midHighCrossover ? audioProcessor.midHighCrossover->get() : 20000.0f; // Default if null
    // float midFreq = ... // Mid frequency isn't directly a parameter here

    // Define colors for markers
    Colour lowColour = Colours::orange;
    // Colour midColour = Colours::lightgreen; // No mid marker in this version
    Colour highColour = Colours::cyan;

    // Lambda to draw a single marker line and dot
    auto drawMarker = [&](float freq, Colour colour)
        {
            float x = left + frequencyToX(freq, width); // Map frequency to X
            // Draw only if within bounds
            if (x >= left && x <= right)
            {
                // Draw vertical line
                g.setColour(colour.withAlpha(0.7f));
                g.drawVerticalLine(roundToInt(x), top, bottom); // Use roundToInt
                // Draw dot at the top
                g.setColour(colour);
                g.fillEllipse(x - 2.f, top - 2.f, 4.f, 4.f); // Center dot slightly above top line maybe?
            }
        };

    // Draw markers for the low and high crossover frequencies
    drawMarker(lowFreq, lowColour);
    // drawMarker(midFreq, midColour); // No mid marker
    drawMarker(highFreq, highColour);
}


float SpectrumAnalyzer::frequencyToX(float freq, float width) const
{
    // Clamp frequency to the displayable range
    freq = juce::jlimit(minFreq, maxFreq, freq);
    // Handle edge cases to avoid issues with log
    if (maxFreq / minFreq <= 1.0f || freq <= 0 || width <= 0) return 0.0f;
    // Logarithmic mapping: log(f / min) / log(max / min) gives position [0, 1]
    return width * (std::log(freq / minFreq) / std::log(maxFreq / minFreq));
}


float SpectrumAnalyzer::xToFrequency(float x, float width) const
{
    // Clamp x coordinate within the width
    x = juce::jlimit(0.0f, width, x);
    // Handle edge cases
    if (maxFreq / minFreq <= 1.0f || width <= 0.0f) return minFreq;
    // Inverse logarithmic mapping: min * exp((x / width) * log(max / min))
    return minFreq * std::exp((x / width) * std::log(maxFreq / minFreq));
}