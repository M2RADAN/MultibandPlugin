#include "AnalyzerOverlay.h"
#include "../Source/PluginProcessor.h" // Âêëþ÷àåì, åñëè íóæíû êîíñòàíòû sampleRate è ò.ä.
                                     // íî äëÿ äàííîé ðåàëèçàöèè îí íå îáÿçàòåëåí.

namespace MBRP_GUI
{

    // Êîíñòðóêòîð
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
        juce::AudioParameterFloat& midXover) :
        lowMidXoverParam(lowXover),
        midHighXoverParam(midXover)
    {
        // Îâåðëåé äîëæåí ïåðåõâàòûâàòü ñîáûòèÿ ìûøè, ÷òîáû ðåàãèðîâàòü íà êëèêè/ïåðåòàñêèâàíèÿ
        setInterceptsMouseClicks(true, true);
        setPaintingIsUnclipped(true);
        startTimerHz(30); // Òàéìåð äëÿ ïåðåðèñîâêè ëèíèé ïðè èçìåíåíèè ïàðàìåòðîâ
    }

    // Îòðèñîâêà
    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        // Ðèñóåì òîëüêî ëèíèè êðîññîâåðà, èñïîëüçóÿ ãðàíèöû ÃÐÀÔÈÊÀ
        drawCrossoverLines(g, getGraphBounds());
    }

    // Òàéìåð (ïðîñòî ïåðåðèñîâûâàåò)
    void AnalyzerOverlay::timerCallback()
    {
        // Ïåðåðèñîâûâàåì, ÷òîáû ëèíèè îáíîâëÿëèñü, åñëè ïàðàìåòðû èçìåíèëèñü èçâíå
        repaint();
    }

    // Èçìåíåíèå ðàçìåðà (ïðîñòî ïåðåðèñîâûâàåì)
    void AnalyzerOverlay::resized()
    {
        repaint();
    }

    // --- Ïîëó÷åíèå îáëàñòè ãðàôèêà ---
    // ÂÀÆÍÎ: Ýòà ëîãèêà ÄÎËÆÍÀ ÑÎÂÏÀÄÀÒÜ ñ òåì, êàê SpectrumAnalyzer îïðåäåëÿåò ñâîþ îáëàñòü
    juce::Rectangle<float> AnalyzerOverlay::getGraphBounds() const
    {
        // Ïðèìåð ëîãèêè, êàê áûëî â SpectrumAnalyzer::drawGrid
        // Çàìåíèòå íà àêòóàëüíóþ ëîãèêó âàøåãî SpectrumAnalyzer, åñëè îíà îòëè÷àåòñÿ!
        return getLocalBounds().toFloat().reduced(1.f, 5.f);
    }


    // --- Ïðåîáðàçîâàíèå X â ÷àñòîòó ---
    float AnalyzerOverlay::xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const
    {
        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();
        // Èñïîëüçóåì mapXToFreqLog èç õåäåðà
        float freq = mapXToFreqLog(x, left, width, minLogFreq, maxLogFreq);
        // Äîïîëíèòåëüíî îãðàíè÷èâàåì äèàïàçîíîì ïàðàìåòðîâ (íà âñÿêèé ñëó÷àé)
        freq = juce::jlimit(lowMidXoverParam.getNormalisableRange().start, // Íèæíèé ïðåäåë
            midHighXoverParam.getNormalisableRange().end,  // Âåðõíèé ïðåäåë
            freq);
        return freq;
    }

    // --- Îòðèñîâêà ëèíèé êðîññîâåðà ---
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

        // Èñïîëüçóåì mapFreqToXLog èç õåäåðà
        float lowMidX = mapFreqToXLog(lowMidFreq, left, width, minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighFreq, left, width, minLogFreq, maxLogFreq);

        // Ðèñóåì ëèíèè
        g.setColour(crossoverLineColour.withAlpha(0.7f));
        if (lowMidX >= left && lowMidX <= right) g.drawVerticalLine(roundToInt(lowMidX), top, bottom);

        g.setColour(crossoverLineColour2.withAlpha(0.7f));
        if (midHighX >= left && midHighX <= right) g.drawVerticalLine(roundToInt(midHighX), top, bottom);

        // Ðèñóåì ðó÷êè
        const float handleSize = 6.0f;
        const float handleRadius = handleSize / 2.0f;
        g.setColour(crossoverLineColour);
        if (lowMidX >= left && lowMidX <= right) g.fillEllipse(lowMidX - handleRadius, top, handleSize, handleSize);
        g.setColour(crossoverLineColour2);
        if (midHighX >= left && midHighX <= right) g.fillEllipse(midHighX - handleRadius, top, handleSize, handleSize);
    }

    // --- Îáðàáîò÷èêè ìûøè ---
    void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    {
        auto graphBounds = getGraphBounds(); // Ïîëó÷àåì îáëàñòü ãðàôèêà
        if (!graphBounds.contains(event.getPosition().toFloat())) // Êëèê âíå îáëàñòè ãðàôèêà
        {
            currentDraggingState = DraggingState::None;
            return;
        }

        float mouseX = static_cast<float>(event.x);

        // Ïîëó÷àåì òåêóùèå X-êîîðäèíàòû ëèíèé âíóòðè îáëàñòè ãðàôèêà
        float lowMidX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

        // Ïðîâåðÿåì êëèê ðÿäîì ñ ëèíèÿìè
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
        else { // Êëèê ïî îáëàñòè
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
        repaint(); // Îáíîâèòü âèä (íàïðèìåð, êóðñîð)
    }

    void AnalyzerOverlay::mouseDrag(const juce::MouseEvent& event)
    {
        if (currentDraggingState == DraggingState::None) return;

        auto graphBounds = getGraphBounds();
        // Îãðàíè÷èâàåì X êîîðäèíàòó ãðàíèöàìè ãðàôèêà ïåðåä êîíâåðòàöèåé â ÷àñòîòó
        float mouseX = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), (float)event.x);
        float newFreq = xToFrequency(mouseX, graphBounds);

        if (currentDraggingState == DraggingState::DraggingLowMid) {
            newFreq = juce::jlimit(minLogFreq, midHighXoverParam.get() - 1.0f, newFreq); // Îãðàíè÷åíèå
            lowMidXoverParam.setValueNotifyingHost(lowMidXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            newFreq = juce::jlimit(lowMidXoverParam.get() + 1.0f, maxLogFreq, newFreq); // Îãðàíè÷åíèå
            midHighXoverParam.setValueNotifyingHost(midHighXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }
        // Ïåðåðèñîâêà áóäåò ïî òàéìåðó èëè ìîæíî äîáàâèòü repaint()
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