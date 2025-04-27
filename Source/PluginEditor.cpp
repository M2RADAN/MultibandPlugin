#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- ControlBar Implementation ---
ControlBar::ControlBar()
{
    // The button state doesn't control the new analyzer directly
    // analyzerButton.setToggleState(true, juce::NotificationType::dontSendNotification);
    addAndMakeVisible(analyzerButton);
}

void ControlBar::resized()
{
    auto bounds = getLocalBounds();
    // Use roundToInt for bounds setting from float calculations if needed,
    // but here it's direct integer math.
    analyzerButton.setBounds(bounds.removeFromLeft(50)
        .withTrimmedTop(4)
        .withTrimmedLeft(4));
}
// ---------------------------

// --- AnalyzerOverlay Implementation ---
namespace MBRP_GUI
{
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
        juce::AudioParameterFloat& midXover) :
        lowMidXoverParam(lowXover),
        midHighXoverParam(midXover),
        lastLowMidFreq(lowXover.get() + 1.f), // Initialize to ensure first repaint
        lastMidHighFreq(midXover.get() + 1.f)
    {
        setInterceptsMouseClicks(false, false); // Overlay shouldn't block mouse events
        startTimerHz(30); // Timer for smooth line updates
    }

    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        // Draw the crossover lines based on current parameters
        drawCrossoverLines(g, getLocalBounds());
    }

    void AnalyzerOverlay::timerCallback()
    {
        // --- UPDATED: Repaint only if values changed ---
        float currentLowMid = lowMidXoverParam.get();
        float currentMidHigh = midHighXoverParam.get();

        if (!juce::approximatelyEqual(currentLowMid, lastLowMidFreq) ||
            !juce::approximatelyEqual(currentMidHigh, lastMidHighFreq))
        {
            lastLowMidFreq = currentLowMid;
            lastMidHighFreq = currentMidHigh;
            repaint(); // Request repaint only when frequencies change
        }
        // ---------------------------------------------
    }

    void AnalyzerOverlay::resized()
    {
        // Request repaint when size changes, as drawing depends on bounds
        repaint();
    }

    // Helper function to get the drawing area (matching SpectrumAnalyzer)
    juce::Rectangle<int> AnalyzerOverlay::getAnalysisArea(juce::Rectangle<int> bounds) const
    {
        // Reduce bounds similarly to SpectrumAnalyzer::paint/drawGrid
        bounds.reduce(5, 5);
        // Optional: Further reduce based on label space if needed for exact alignment
        // bounds.removeFromLeft(20);
        // bounds.removeFromRight(30);
        // bounds.removeFromBottom(15);
        return bounds;
    }


    // Helper function for logarithmic mapping (copied or from Utilities)
    template<typename FloatType>
    static FloatType mapFreqToXLog(FloatType freq, FloatType left, FloatType width, FloatType minF, FloatType maxF)
    {
        freq = juce::jlimit(minF, maxF, freq);
        if (maxF / minF <= 1.0f || freq <= 0) return left; // Avoid log(0) or division by zero
        return left + width * (std::log(freq / minF) / std::log(maxF / minF));
    }


    void AnalyzerOverlay::drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        auto analysisArea = getAnalysisArea(bounds).toFloat(); // Get float bounds for drawing
        const float top = analysisArea.getY();
        const float bottom = analysisArea.getBottom();
        const float left = analysisArea.getX();
        const float width = analysisArea.getWidth();
        const float right = analysisArea.getRight();

        // Define frequency range for mapping (should match SpectrumAnalyzer)
        const float minLogFreq = 20.0f;
        const float maxLogFreq = 20000.0f;

        // Get current frequencies from parameters
        float lowMidFreq = lowMidXoverParam.get();
        float midHighFreq = midHighXoverParam.get();

        // Map frequencies to X coordinates using logarithmic scale
        float lowMidX = mapFreqToXLog(lowMidFreq, left, width, minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighFreq, left, width, minLogFreq, maxLogFreq);

        g.setColour(Colours::lightblue.withAlpha(0.7f)); // Set line color and alpha

        // Draw lines only if they are within the visible analysis area bounds
        if (lowMidX >= left && lowMidX <= right)
            g.drawVerticalLine(roundToInt(lowMidX), top, bottom); // Use roundToInt for pixel accuracy

        if (midHighX >= left && midHighX <= right)
            g.drawVerticalLine(roundToInt(midHighX), top, bottom);
    }

} // End namespace MBRP_GUI
// ---------------------------


//==============================================================================
MBRPAudioProcessorEditor::MBRPAudioProcessorEditor(MBRPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    analyzer(p), // Initialize new analyzer
    // Pass crossover parameters to the overlay
    analyzerOverlay(*p.lowMidCrossover, *p.midHighCrossover),
    // Initialize panSlider (rap will be set in updatePanAttachment)
    panSlider(/*rap=*/nullptr, /*unitSuffix=*/"", /*title=*/"Pan"),
    // Initialize attachments for crossover sliders
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider)
    // panAttachment is initialized later via unique_ptr
{
    setLookAndFeel(&lnf); // Set custom LookAndFeel

    // Add main components to the editor
    addAndMakeVisible(controlBar);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(analyzerOverlay); // Overlay draws on top of analyzer
    addAndMakeVisible(bandSelectControls);

    // Lambda function to configure standard sliders
    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText,
        juce::Slider::SliderStyle style, juce::Colour colour)
        {
            slider.setSliderStyle(style);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20);
            slider.setPopupDisplayEnabled(true, false, this);
            slider.setColour(juce::Slider::thumbColourId, colour);
            slider.setColour(juce::Slider::trackColourId, colour.darker(0.5f));
            slider.setName(labelText);
            addAndMakeVisible(slider);

            label.setText(labelText, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.attachToComponent(&slider, false);
            // --- UPDATED: Use FontOptions ---
            label.setFont(juce::Font(juce::FontOptions(12.0f)));
            // ------------------------------
            label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(label);
        };

    // Setup crossover sliders
    setupStandardSlider(lowMidCrossoverSlider, lowMidCrossoverLabel, "Low / Mid", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);
    setupStandardSlider(midHighCrossoverSlider, midHighCrossoverLabel, "Mid / High", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);

    // Setup the single pan slider
    panSlider.setPopupDisplayEnabled(true, false, this);
    addAndMakeVisible(panSlider);
    panLabel.setText("Pan", juce::dontSendNotification); // Initial text
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.attachToComponent(&panSlider, false);
    // --- UPDATED: Use FontOptions ---
    panLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    // ------------------------------
    panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(panLabel);

    // Connect band selection to the update function
    bandSelectControls.onBandSelected = [this](int bandIndex) {
        updatePanAttachment(bandIndex);
        };

    // Initialize pan slider attachment for the default band (Low)
    updatePanAttachment(0);

    // Set editor size
    setSize(600, 700);
    // Start editor timer (if needed for other UI updates)
    startTimerHz(60); // Reduced frequency, maybe 30 is enough?
}


MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor()
{
    setLookAndFeel(nullptr); // Reset LookAndFeel
}

//==============================================================================
void MBRPAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill background with color from LookAndFeel
    g.fillAll(lnf.findColour(juce::ResizableWindow::backgroundColourId));
    // Components draw themselves
}

void MBRPAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds(); // Get total editor area
    int padding = 10; // Padding around elements

    // Position ControlBar at the top
    controlBar.setBounds(bounds.removeFromTop(32));

    // Position Analyzer and Overlay in the upper section
    int analyzerHeight = 250;
    auto analyzerArea = bounds.removeFromTop(analyzerHeight);
    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzerArea); // Overlay sits directly on top

    // Area for remaining controls
    auto controlsArea = bounds.reduced(padding);
    controlsArea.removeFromTop(padding); // Extra top padding

    // Position Crossover Sliders
    int crossoverHeight = 50;
    auto crossoverArea = controlsArea.removeFromTop(crossoverHeight);
    int crossoverWidth = crossoverArea.getWidth() / 2;
    lowMidCrossoverSlider.setBounds(crossoverArea.removeFromLeft(crossoverWidth).reduced(padding / 2));
    midHighCrossoverSlider.setBounds(crossoverArea.reduced(padding / 2));

    controlsArea.removeFromTop(padding); // Space

    // Position Band Select Controls
    int bandSelectHeight = 30;
    bandSelectControls.setBounds(controlsArea.removeFromTop(bandSelectHeight));

    controlsArea.removeFromTop(padding / 2); // Space

    // Position the single Pan Slider in the remaining area (centered)
    auto panArea = controlsArea;
    panSlider.setBounds(panArea.reduced(panArea.getWidth() / 4, padding));
    // panLabel is positioned automatically by attachToComponent
}

void MBRPAudioProcessorEditor::timerCallback()
{
    // This timer can be used for custom UI elements that need periodic updates.
    // The SpectrumAnalyzer and AnalyzerOverlay have their own timers.
    // Can be left empty if not needed.
}

// --- Function to update the pan slider's attachment and appearance ---
void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex)
{
    juce::String paramId;
    juce::String labelText;
    juce::Colour color;

    // Determine parameter ID, label text, and color based on the selected band
    switch (bandIndex)
    {
    case 0: // Low
        paramId = "lowPan";
        labelText = "Low Pan";
        color = juce::Colours::orange;
        break;
    case 1: // Mid
        paramId = "midPan";
        labelText = "Mid Pan";
        color = juce::Colours::lightgreen;
        break;
    case 2: // High
        paramId = "highPan";
        labelText = "High Pan";
        color = juce::Colours::cyan;
        break;
    default:
        jassertfalse; // Invalid band index
        return;
    }

    // 1. Get the target parameter from APVTS
    auto* targetParam = processorRef.getAPVTS().getParameter(paramId);
    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
    jassert(rangedParam != nullptr); // Ensure parameter exists and is ranged
    if (!rangedParam) return;

    // 2. Update the RotarySliderWithLabels internal parameter reference
    panSlider.changeParam(rangedParam);

    // 3. Update labels specific to the pan slider (L/C/R)
    panSlider.labels.clear();
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20); // Keep textbox below
    panSlider.labels.add({ 0.0f, "L" }); // Position 0.0 corresponds to value -1.0
    panSlider.labels.add({ 0.5f, "C" }); // Position 0.5 corresponds to value 0.0
    panSlider.labels.add({ 1.0f, "R" }); // Position 1.0 corresponds to value +1.0

    // 4. Reset the old attachment (unique_ptr handles deletion)
    panAttachment.reset();
    // 5. Create a new attachment linking the slider to the chosen parameter ID
    panAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramId, panSlider);

    // 6. Update the external label text and slider colors
    panLabel.setText(labelText, juce::dontSendNotification);
    panSlider.setColour(juce::Slider::thumbColourId, color);
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, color.withAlpha(0.7f));

    // 7. Repaint the slider to apply visual changes
    panSlider.repaint();
}
// ----------------------------------------------------------------------