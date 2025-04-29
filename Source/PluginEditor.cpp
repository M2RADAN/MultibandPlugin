#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- ControlBar Implementation ---
ControlBar::ControlBar() {
    addAndMakeVisible(analyzerButton);
}
void ControlBar::resized() {
    auto bounds = getLocalBounds();
    analyzerButton.setBounds(bounds.removeFromLeft(50).withTrimmedTop(4).withTrimmedLeft(4));
}
// ---------------------------

//// --- AnalyzerOverlay Implementation ---
//namespace MBRP_GUI
//{
//    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover, juce::AudioParameterFloat& midXover) :
//        lowMidXoverParam(lowXover), midHighXoverParam(midXover),
//        lastLowMidFreq(lowXover.get() + 1.f), lastMidHighFreq(midXover.get() + 1.f)
//    {
//        setInterceptsMouseClicks(false, false);
//        startTimerHz(30);
//    }
//
//    void AnalyzerOverlay::paint(juce::Graphics& g) {
//        drawCrossoverLines(g, getLocalBounds());
//    }
//
//    void AnalyzerOverlay::timerCallback() {
//        float currentLowMid = lowMidXoverParam.get();
//        float currentMidHigh = midHighXoverParam.get();
//        if (!juce::approximatelyEqual(currentLowMid, lastLowMidFreq) || !juce::approximatelyEqual(currentMidHigh, lastMidHighFreq)) {
//            lastLowMidFreq = currentLowMid;
//            lastMidHighFreq = currentMidHigh;
//            repaint();
//        }
//    }
//
//    void AnalyzerOverlay::resized() { repaint(); }
//
//    juce::Rectangle<int> AnalyzerOverlay::getAnalysisArea(juce::Rectangle<int> bounds) const {
//        bounds.reduce(1, 5); // Óìåíüøàåì îòñòóïû êàê â SpectrumAnalyzer::drawGrid
//        return bounds;
//    }
//
//    template<typename FloatType>
//    static FloatType mapFreqToXLog(FloatType freq, FloatType left, FloatType width, FloatType minF, FloatType maxF) {
//        freq = juce::jlimit(minF, maxF, freq);
//        if (maxF / minF <= 1.0f || freq <= 0) return left;
//        return left + width * (std::log(freq / minF) / std::log(maxF / minF));
//    }
//
//    void AnalyzerOverlay::drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds) {
//        using namespace juce;
//        auto analysisArea = getAnalysisArea(bounds).toFloat();
//        const float top = analysisArea.getY();
//        const float bottom = analysisArea.getBottom();
//        const float left = analysisArea.getX();
//        const float width = analysisArea.getWidth();
//        const float right = analysisArea.getRight();
//        const float minLogFreq = 20.0f;
//        const float maxLogFreq = 20000.0f; // Èëè processorRef.getSampleRate() / 2.0f;
//
//        float lowMidFreq = lowMidXoverParam.get();
//        float midHighFreq = midHighXoverParam.get();
//        float lowMidX = mapFreqToXLog(lowMidFreq, left, width, minLogFreq, maxLogFreq);
//        float midHighX = mapFreqToXLog(midHighFreq, left, width, minLogFreq, maxLogFreq);
//
//        g.setColour(Colours::orange.withAlpha(0.7f)); // Öâåò äëÿ Low/Mid ëèíèè
//        if (lowMidX >= left && lowMidX <= right) g.drawVerticalLine(roundToInt(lowMidX), top, bottom);
//
//        g.setColour(Colours::cyan.withAlpha(0.7f)); // Öâåò äëÿ Mid/High ëèíèè
//        if (midHighX >= left && midHighX <= right) g.drawVerticalLine(roundToInt(midHighX), top, bottom);
//
//        // Äîáàâëÿåì òî÷êè íà ëèíèÿõ êðîññîâåðà
//        g.setColour(Colours::orange);
//        if (lowMidX >= left && lowMidX <= right) g.fillEllipse(lowMidX - 2.f, top, 4.f, 4.f);
//        g.setColour(Colours::cyan);
//        if (midHighX >= left && midHighX <= right) g.fillEllipse(midHighX - 2.f, top, 4.f, 4.f);
//    }
//} // End namespace MBRP_GUI
// ---------------------------

//==============================================================================
MBRPAudioProcessorEditor::MBRPAudioProcessorEditor(MBRPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p),
    analyzer(p), // Èíèöèàëèçàöèÿ íîâîãî àíàëèçàòîðà
    analyzerOverlay(*p.lowMidCrossover, *p.midHighCrossover),
    panSlider(nullptr, "", "Pan"),
    lowMidCrossoverAttachment(processorRef.getAPVTS(), "lowMidCrossover", lowMidCrossoverSlider),
    midHighCrossoverAttachment(processorRef.getAPVTS(), "midHighCrossover", midHighCrossoverSlider)
{
    setLookAndFeel(&lnf);
    processorRef.setCopyToFifo(true); // Âêëþ÷àåì FIFO äëÿ àíàëèçàòîðà

    addAndMakeVisible(controlBar);
    addAndMakeVisible(analyzer);
    addAndMakeVisible(analyzerOverlay);
    addAndMakeVisible(bandSelectControls);

    auto setupStandardSlider = [&](juce::Slider& slider, juce::Label& label, const juce::String& labelText,
        juce::Slider::SliderStyle style, juce::Colour colour) {
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
            label.setFont(juce::Font(juce::FontOptions(12.0f)));
            label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(label);
        };

    setupStandardSlider(lowMidCrossoverSlider, lowMidCrossoverLabel, "Low / Mid", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);
    setupStandardSlider(midHighCrossoverSlider, midHighCrossoverLabel, "Mid / High", juce::Slider::LinearHorizontal, juce::Colours::lightgrey);

    panSlider.setPopupDisplayEnabled(true, false, this);
    addAndMakeVisible(panSlider);
    panLabel.setText("Pan", juce::dontSendNotification);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.attachToComponent(&panSlider, false);
    panLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(panLabel);
    analyzerOverlay.onBandAreaClicked = [this](int bandIndex) {
        // Эта лямбда будет вызвана, когда пользователь кликнет на область в оверлее
        bandSelectControls.setSelectedBand(bandIndex); // Обновляем кнопки
        updatePanAttachment(bandIndex);             // Обновляем слайдер панорамы
        };
    // ------------------------------------

    // Подключаем колбэк от КНОПОК выбора полосы (остается)
    bandSelectControls.onBandSelected = [this](int bandIndex) {
        updatePanAttachment(bandIndex); // Обновляем слайдер панорамы при клике на кнопку
        };

    updatePanAttachment(0); // Инициализация для Low

    setSize(900, 700);
    startTimerHz(30);
}

MBRPAudioProcessorEditor::~MBRPAudioProcessorEditor() {
    processorRef.setCopyToFifo(false); // Âûêëþ÷àåì FIFO
    setLookAndFeel(nullptr);
}

void MBRPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(lnf.findColour(juce::ResizableWindow::backgroundColourId));
}

void MBRPAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds();
    int padding = 10;
    controlBar.setBounds(bounds.removeFromTop(32));
    int analyzerHeight = 350; // Íîâàÿ âûñîòà
    auto analyzerArea = bounds.removeFromTop(analyzerHeight);
    analyzer.setBounds(analyzerArea);
    analyzerOverlay.setBounds(analyzerArea);
    auto controlsArea = bounds.reduced(padding);
    controlsArea.removeFromTop(padding);
    int crossoverHeight = 50;
    auto crossoverArea = controlsArea.removeFromTop(crossoverHeight);
    int crossoverWidth = crossoverArea.getWidth() / 2;
    lowMidCrossoverSlider.setBounds(crossoverArea.removeFromLeft(crossoverWidth).reduced(padding / 2));
    midHighCrossoverSlider.setBounds(crossoverArea.reduced(padding / 2));
    controlsArea.removeFromTop(padding);
    int bandSelectHeight = 30;
    bandSelectControls.setBounds(controlsArea.removeFromTop(bandSelectHeight));
    controlsArea.removeFromTop(padding / 2);
    auto panArea = controlsArea;
    panSlider.setBounds(panArea.reduced(panArea.getWidth() / 4, padding));
}

void MBRPAudioProcessorEditor::timerCallback() {
    // Îñòàâëÿåì ïóñòûì, ò.ê. êîìïîíåíòû èìåþò ñâîè òàéìåðû
}

void MBRPAudioProcessorEditor::updatePanAttachment(int bandIndex) {
    juce::String paramId;
    juce::String labelText;
    juce::Colour color;
    switch (bandIndex) {
    case 0: paramId = "lowPan"; labelText = "Low Pan"; color = juce::Colours::orange; break;
    case 1: paramId = "midPan"; labelText = "Mid Pan"; color = juce::Colours::lightgreen; break;
    case 2: paramId = "highPan"; labelText = "High Pan"; color = juce::Colours::cyan; break;
    default: jassertfalse; return;
    }
    auto* targetParam = processorRef.getAPVTS().getParameter(paramId);
    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(targetParam);
    jassert(rangedParam != nullptr); if (!rangedParam) return;
    panSlider.changeParam(rangedParam);
    panSlider.labels.clear();
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    panSlider.labels.add({ 0.0f, "L" }); panSlider.labels.add({ 0.5f, "C" }); panSlider.labels.add({ 1.0f, "R" });
    panAttachment.reset();
    panAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), paramId, panSlider);
    panLabel.setText(labelText, juce::dontSendNotification);
    panSlider.setColour(juce::Slider::thumbColourId, color);
    panSlider.setColour(juce::Slider::rotarySliderFillColourId, color.withAlpha(0.7f));
    panSlider.repaint();
}