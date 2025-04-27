#include "AnalyzerOverlay.h"
//#include "../Source/PluginProcessor.h" // ��������, ���� ����� ��������� sampleRate � �.�.
                                     // �� ��� ������ ���������� �� �� ����������.

namespace MBRP_GUI
{

    // �����������
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
        juce::AudioParameterFloat& midXover) :
        lowMidXoverParam(lowXover),
        midHighXoverParam(midXover)
    {
        // ������� ������ ������������� ������� ����, ����� ����������� �� �����/��������������
        setInterceptsMouseClicks(true, true);
        setPaintingIsUnclipped(true);
        startTimerHz(30); // ������ ��� ����������� ����� ��� ��������� ����������
    }

    // ���������
    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        // ������ ������ ����� ����������, ��������� ������� �������
        drawCrossoverLines(g, getGraphBounds());
    }

    // ������ (������ ��������������)
    void AnalyzerOverlay::timerCallback()
    {
        // ��������������, ����� ����� �����������, ���� ��������� ���������� �����
        repaint();
    }

    // ��������� ������� (������ ��������������)
    void AnalyzerOverlay::resized()
    {
        repaint();
    }

    // --- ��������� ������� ������� ---
    // �����: ��� ������ ������ ��������� � ���, ��� SpectrumAnalyzer ���������� ���� �������
    juce::Rectangle<float> AnalyzerOverlay::getGraphBounds() const
    {
        // ������ ������, ��� ���� � SpectrumAnalyzer::drawGrid
        // �������� �� ���������� ������ ������ SpectrumAnalyzer, ���� ��� ����������!
        return getLocalBounds().toFloat().reduced(1.f, 5.f);
    }


    // --- �������������� X � ������� ---
    float AnalyzerOverlay::xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const
    {
        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();
        // ���������� mapXToFreqLog �� ������
        float freq = mapXToFreqLog(x, left, width, minLogFreq, maxLogFreq);
        // ������������� ������������ ���������� ���������� (�� ������ ������)
        freq = juce::jlimit(lowMidXoverParam.getNormalisableRange().start, // ������ ������
            midHighXoverParam.getNormalisableRange().end,  // ������� ������
            freq);
        return freq;
    }

    // --- ��������� ����� ���������� ---
    void AnalyzerOverlay::drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        using namespace juce;
        auto width = graphBounds.getWidth();
        auto top = graphBounds.getY();
        auto bottom = graphBounds.getBottom();
        auto left = graphBounds.getX();
        auto right = graphBounds.getRight();

        float lowMidFreq = lowMidXoverParam.get();
        float midHighFreq = midHighXoverParam.get();

        // ���������� mapFreqToXLog �� ������
        float lowMidX = mapFreqToXLog(lowMidFreq, left, width, minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighFreq, left, width, minLogFreq, maxLogFreq);

        // ������ �����
        g.setColour(crossoverLineColour.withAlpha(0.7f));
        if (lowMidX >= left && lowMidX <= right) g.drawVerticalLine(roundToInt(lowMidX), top, bottom);

        g.setColour(crossoverLineColour2.withAlpha(0.7f));
        if (midHighX >= left && midHighX <= right) g.drawVerticalLine(roundToInt(midHighX), top, bottom);

        // ������ �����
        const float handleSize = 6.0f;
        const float handleRadius = handleSize / 2.0f;
        g.setColour(crossoverLineColour);
        if (lowMidX >= left && lowMidX <= right) g.fillEllipse(lowMidX - handleRadius, top, handleSize, handleSize);
        g.setColour(crossoverLineColour2);
        if (midHighX >= left && midHighX <= right) g.fillEllipse(midHighX - handleRadius, top, handleSize, handleSize);
    }

    // --- ����������� ���� ---
    void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    {
        auto graphBounds = getGraphBounds(); // �������� ������� �������
        if (!graphBounds.contains(event.getPosition().toFloat())) // ���� ��� ������� �������
        {
            currentDraggingState = DraggingState::None;
            return;
        }

        float mouseX = static_cast<float>(event.x);

        // �������� ������� X-���������� ����� ������ ������� �������
        float lowMidX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

        // ��������� ���� ����� � �������
        if (std::abs(mouseX - lowMidX) < dragTolerance) {
            currentDraggingState = DraggingState::DraggingLowMid;
            lowMidXoverParam.beginChangeGesture();
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            DBG("Overlay: Started dragging Low/Mid");
        }
        else if (std::abs(mouseX - midHighX) < dragTolerance) {
            currentDraggingState = DraggingState::DraggingMidHigh;
            midHighXoverParam.beginChangeGesture();
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            DBG("Overlay: Started dragging Mid/High");
        }
        else { // ���� �� �������
            currentDraggingState = DraggingState::None;
            float clickedFreq = xToFrequency(mouseX, graphBounds);
            int bandIndex = -1;
            if (clickedFreq < lowMidXoverParam.get()) bandIndex = 0;
            else if (clickedFreq < midHighXoverParam.get()) bandIndex = 1;
            else bandIndex = 2;

            if (onBandAreaClicked) {
                onBandAreaClicked(bandIndex);
            }
            DBG("Overlay: Clicked in band area: " << bandIndex);
        }
        repaint(); // �������� ��� (��������, ������)
    }

    void AnalyzerOverlay::mouseDrag(const juce::MouseEvent& event)
    {
        if (currentDraggingState == DraggingState::None) return;

        auto graphBounds = getGraphBounds();
        // ������������ X ���������� ��������� ������� ����� ������������ � �������
        float mouseX = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), (float)event.x);
        float newFreq = xToFrequency(mouseX, graphBounds);

        if (currentDraggingState == DraggingState::DraggingLowMid) {
            newFreq = juce::jlimit(minLogFreq, midHighXoverParam.get() - 1.0f, newFreq); // �����������
            lowMidXoverParam.setValueNotifyingHost(lowMidXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            newFreq = juce::jlimit(lowMidXoverParam.get() + 1.0f, maxLogFreq, newFreq); // �����������
            midHighXoverParam.setValueNotifyingHost(midHighXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }
        // ����������� ����� �� ������� ��� ����� �������� repaint()
    }

    void AnalyzerOverlay::mouseUp(const juce::MouseEvent& /*event*/)
    {
        if (currentDraggingState == DraggingState::DraggingLowMid) {
            lowMidXoverParam.endChangeGesture();
            DBG("Overlay: Ended dragging Low/Mid");
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            midHighXoverParam.endChangeGesture();
            DBG("Overlay: Ended dragging Mid/High");
        }
        currentDraggingState = DraggingState::None;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

} // namespace MBRP_GUI