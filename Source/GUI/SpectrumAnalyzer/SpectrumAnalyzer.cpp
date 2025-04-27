#include "SpectrumAnalyzer.h" // �������� PluginProcessor.h ����� ����
#include <vector>
#include <cmath>      // ��� std::log, std::exp, std::abs, std::max
#include <limits>     // �� ������������ ��������, �� ������� ��� �������� ��������
#include <algorithm>  // ��� std::min, std::max, std::sort, std::unique
#include <juce_gui_basics/juce_gui_basics.h> // ��� TextLayout, Graphics � �.�.
#include <juce_dsp/juce_dsp.h>             // ��� Decibels

// ��������, ��� ������������ ���� �� LookAndFeel �������� ��� ������ (���� �����)
// #include "../LookAndFeel.h" // ����������������, ���� ColorScheme ��������� ���

// --- ������������ ���� ---
namespace MBRP_GUI
{

    // --- ������ ��� ������ ---
    // ����������� ����������� �������-�����
    /*static*/float SpectrumAnalyzer::getTextLayoutWidth(const juce::String& text, const juce::Font& font)
    {
        juce::TextLayout textLayout;
        juce::AttributedString attrString(text);
        attrString.setFont(font);
        textLayout.createLayout(attrString, 10000.0f); // Use a large width to get the natural width
        return textLayout.getWidth();
    }

    //==============================================================================
    // �����������
    SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& p) :
        processor{ p },
        // �������������� ������� � �������� fftSize/2 � ��������� mindB
        displayData(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        peakHoldLevels(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        fftPoints(MBRPAudioProcessor::fftSize) // �������� ������ ��� fftPoints (��������� �������� int)
    {
        peakDbLevel.store(mindB);   // �������������� ��������� ���
        avgInput.clear();           // ������� ������ ����������
        avgOutput.clear();
        startTimerHz(60);           // ������������� ������� ������� (��������, 60 ��)
    }

    //==============================================================================
    // ������ - �������� ���� ���������� �����������
    void SpectrumAnalyzer::timerCallback()
    {
        bool newDataAvailable = processor.nextFFTBlockReady.load(); // ��������� ���� �� ����������

        if (newDataAvailable)
        {
            drawNextFrame(); // ������������ ����� ������ FFT �� FIFO
            processor.nextFFTBlockReady.store(false); // ���������� ����
        }

        // ��������� �������� ����� ��������� �������
        if (resizeDebounceInFrames > 0)
        {
            --resizeDebounceInFrames;
            if (resizeDebounceInFrames == 0) // ����� ����������� �����
            {
                recalculateFftPoints();
                // �������� ����������� ����� ���������, ���� ���� �� ���� ����� ������
                if (!newDataAvailable) repaint(); // ������������, ���� ����� ������ �� ����, �� ������ ����������
            }
            // ���� ���� ������/��������, �� ��������� ���� � �� �������������� �������
            return;
        }

        // ��������� ���� (��������� ���������), ���� ���� �� ���� ����� ������ FFT
        bool peakNeedsRepaint = false;
        for (size_t i = 0; i < peakHoldLevels.size(); ++i)
        {
            float oldPeakDb = peakHoldLevels[i];
            // �������� ���, ���� �� ���� �������� ����� � ���� ������ mindB
            if (oldPeakDb > displayData[i] && oldPeakDb > mindB + 0.01f)
            {
                peakHoldLevels[i] = std::max(displayData[i], juce::Decibels::gainToDecibels(juce::Decibels::decibelsToGain(oldPeakDb) * peakHoldDecayFactor, mindB));
                if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) {
                    peakNeedsRepaint = true; // ��������, ��� ��� ���������
                }
            }
            else if (oldPeakDb > mindB + 0.01f && peakHoldLevels[i] < displayData[i]) {
                // ���� ��� ���� ���� �������� �����, ������������� ��� ������ �������� �����
                peakHoldLevels[i] = displayData[i];
                if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) {
                    peakNeedsRepaint = true;
                }
            }
        }

        // ��������������, ���� ���� ����� ������ ��� ���� ���������� ����
        if (newDataAvailable || peakNeedsRepaint) {
            repaint();
        }
    }


    // --- ��������� ���������� ����� ������ �� FIFO ---
    // ������������ �� witte::SpectrumAnalyzer::drawNextFrame
    void SpectrumAnalyzer::drawNextFrame()
    {
        bool newDataProcessed = false; // ���� ��� ��������� ��������� ����� ������ FFT

        // --- ��������� �������� FIFO ---
        while (processor.abstractFifoInput.getNumReady() >= MBRPAudioProcessor::fftSize) // ������������ ��� ��������� �����
        {
            fftBufferInput.clear(); // ������� ��������� ����� ��� FFT
            int start1, block1, start2, block2;
            // ������� FIFO � ������
            processor.abstractFifoInput.prepareToRead(MBRPAudioProcessor::fftSize, start1, block1, start2, block2);

            // --- �������� ������ �� FIFO ���������� �� ��������� ����� ����������� ---
            const int audioFifoSize = processor.audioFifoInput.getNumSamples();
            if (audioFifoSize > 0) { // ���. �������� �� ������ ������ FIFO
                if (block1 > 0) fftBufferInput.copyFrom(0, 0, processor.audioFifoInput.getReadPointer(0, start1 % audioFifoSize), block1);
                if (block2 > 0) fftBufferInput.copyFrom(0, block1, processor.audioFifoInput.getReadPointer(0, start2 % audioFifoSize), block2);
            }
            // --- ---

            // �������� FIFO ����������, ��� �� ��������� ������
            processor.abstractFifoInput.finishedRead(block1 + block2);

            // ��������� ���� �����
            hannWindow.multiplyWithWindowingTable(fftBufferInput.getWritePointer(0), static_cast<size_t>(MBRPAudioProcessor::fftSize));
            // ��������� ��� (������ ���������)
            fftInput.performFrequencyOnlyForwardTransform(fftBufferInput.getWritePointer(0));

            // --- ���������� ���������� FFT ---
            {
                juce::ScopedLock lockedForAvgUpdate(pathCreationLock);
                // �������� ����� ������ ���� �� ����� (����� 0)
                avgInput.addFrom(0, 0, avgInput.getReadPointer(avgInputPtr), avgInput.getNumSamples(), -1.0f);
                // ������������ ��� ���������� (����� �� ���-�� ������)
                float normFactor = 1.0f / (static_cast<float>(avgInput.getNumChannels() - 1));
                avgInput.copyFrom(avgInputPtr, 0, fftBufferInput.getReadPointer(0), avgInput.getNumSamples(), normFactor);
                // ��������� ����� ��������������� ���� � ����� ����� (� ����� 0)
                avgInput.addFrom(0, 0, avgInput.getReadPointer(avgInputPtr), avgInput.getNumSamples());
            }
            // ����������� ��������� �� ��������� ���� ����������
            if (++avgInputPtr >= avgInput.getNumChannels()) avgInputPtr = 1;

            newDataProcessed = true; // ��������, ��� ���������� ����� ������
        }

        // --- ��������� ��������� FIFO (���� ������������) ---
        // while (processor.abstractFifoOutput.getNumReady() >= MBRPAudioProcessor::fftSize) { /* ... */ }


        // --- ���������� ������������ ������, ���� ���� ����� FFT ---
        if (newDataProcessed)
        {
            auto numBins = avgInput.getNumSamples(); // fftSize / 2
            std::vector<float> latestDbData(numBins); // ������ ��� *�������* dB

            if (displayData.size() != numBins) displayData.resize(numBins);
            if (peakHoldLevels.size() != numBins) peakHoldLevels.resize(numBins);

            // �������� ��������� �� ����������� ���������
            const float* averagedMagnitudes = nullptr;
            {
                juce::ScopedLock lockedForAvgRead(pathCreationLock); // ���������� �� ������
                if (avgInput.getNumSamples() > 0) averagedMagnitudes = avgInput.getReadPointer(0);
            }
            if (averagedMagnitudes == nullptr) return; // �� ������� �������� ������

            // --- ����������� �������� (���������!) ---
            const float gainAdjustment = -55.0f; // <-- ��������� ���!
            const float gainMultiplier = juce::Decibels::decibelsToGain(gainAdjustment);
            // -------------------------------------------

            float currentFramePeak = mindB;

            // 1. ��������� ������ �� �� ����������� ��������
            for (size_t i = 0; i < numBins; ++i)
            {
                float finalMagnitude = averagedMagnitudes[i] * gainMultiplier;
                latestDbData[i] = juce::Decibels::gainToDecibels(finalMagnitude, mindB);
                if (latestDbData[i] > currentFramePeak) currentFramePeak = latestDbData[i];
            }
            peakDbLevel.store(currentFramePeak); // ��������� ����� ���

            // 2. ��������� displayData (EMA) � peakHoldLevels (Decay)
            for (size_t i = 0; i < numBins; ++i)
            {
                float newValDb = latestDbData[i];
                float oldDisplayDb = displayData[i];
                float oldPeakDb = peakHoldLevels[i];
                // EMA
                displayData[i] = smoothingAlpha * newValDb + (1.0f - smoothingAlpha) * oldDisplayDb;
                displayData[i] = std::max(mindB, displayData[i]);
                // Peak Hold
                if (newValDb > oldPeakDb) peakHoldLevels[i] = newValDb;
                else peakHoldLevels[i] = std::max(displayData[i], juce::Decibels::gainToDecibels(juce::Decibels::decibelsToGain(oldPeakDb) * peakHoldDecayFactor, mindB));
            }
            // ����������� ����� ������� �� timerCallback ����� ����� ������
        }
    }

    //==============================================================================
    // �������� ����� FFT <-> ����� (������������ �� witte)
    void SpectrumAnalyzer::recalculateFftPoints()
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto width = bounds.getWidth();
        if (width <= 0) { fftPointsSize = 0; return; }

        const auto sampleRate = static_cast<float> (processor.getSampleRate());
        const auto fftSizeHalved = static_cast<int> (MBRPAudioProcessor::fftSize / 2);
        if (sampleRate <= 0 || fftSizeHalved <= 0) { fftPointsSize = 0; return; } // ���. ��������

        const float minLogFreq = 20.0f;
        const float maxLogFreq = sampleRate / 2.0f;
        if (maxLogFreq <= minLogFreq) { fftPointsSize = 0; return; }
        const float logRange = std::log(maxLogFreq / minLogFreq);

        if (fftPoints.size() != MBRPAudioProcessor::fftSize) // ��������, ��� ������ ������� ����������
            fftPoints.resize(MBRPAudioProcessor::fftSize);

        fftPointsSize = 0;
        int lastX = -1;

        for (int i = 1; i < fftSizeHalved; ++i) // �������� � ���� 1
        {
            const float freq = sampleRate * static_cast<float>(i) / static_cast<float>(MBRPAudioProcessor::fftSize);
            if (freq < minLogFreq) continue;

            const float logPos = std::log(freq / minLogFreq) / logRange;
            const int x = juce::roundToInt(logPos * width);

            if (x >= width) break; // ����� �� �������

            if (x > lastX) // ����� ����� �� ������
            {
                if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i - 1;
                if (fftPointsSize < (int)fftPoints.size()) {
                    fftPoints[fftPointsSize].firstBinIndex = i;
                    fftPoints[fftPointsSize].x = x;
                    fftPointsSize++;
                    lastX = x;
                }
                else { jassertfalse; break; } // ������ ������� �������
            }
            if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i; // ��������� ��������� ���
        }
        if (fftPointsSize > 0) { // ������������ ��������� ������
            fftPoints[fftPointsSize - 1].lastBinIndex = std::min(fftPoints[fftPointsSize - 1].lastBinIndex, fftSizeHalved - 1);
        }
        DBG("Recalculated FFT points. Count: " << fftPointsSize << " for width: " << width);
    }

    //==============================================================================
    // ����������� ������� ��������� ������ ��� ����� ������ �� ����������� ��������
    /*static*/ float SpectrumAnalyzer::getFftPointLevel(const float* averagedMagnitudes, const fftPoint& point)
    {
        float maxMagnitude = 0.0f;
        const int numSamples = MBRPAudioProcessor::fftSize / 2;

        for (int i = point.firstBinIndex; i <= point.lastBinIndex; ++i) {
            if (i >= 0 && i < numSamples) { // �������� ������
                if (averagedMagnitudes[i] > maxMagnitude) maxMagnitude = averagedMagnitudes[i];
            }
        }
        // �������� gainAdjustment ����������� � drawNextFrame ����� ������������ � dB
        return juce::Decibels::gainToDecibels(maxMagnitude, mindB); // ������������ � dB �����? ��� ���������� dB?
        // ���� �������� � drawNextFrame, �� ����� ������ �����������.
        // ��: � drawNextFrame ��� �������������� � latestDbData!
        // ������, getFftPointLevel ������ �� �����, ���� drawSpectrumAndPeaks
        // ����� �������� � displayData/peakHoldLevels ��������.
        // ������� ����, �� ��������, ��� �� ������������.
    }

    //==============================================================================
    // �������� ������� ���������
    void SpectrumAnalyzer::paint(juce::Graphics& g)
    {
        using namespace juce;
        g.fillAll(backgroundColour);
        auto bounds = getLocalBounds().toFloat();
        auto graphBounds = bounds.reduced(1.f);

        // �������� �����, ���� ���� ���� ������ ��� �������/��������
        if (fftPointsSize == 0 && getWidth() > 0) {
            recalculateFftPoints();
        }

        drawFrequencyGrid(g, graphBounds);
        drawGainScale(g, graphBounds);
        drawSpectrumAndPeaks(g, graphBounds);
        drawFrequencyMarkers(g, graphBounds);

        g.setColour(peakTextColour);
        auto peakFont = juce::Font(juce::FontOptions(12.0f)); g.setFont(peakFont);
        float currentPeak = peakDbLevel.load();
        String peakText = "Peak: " + ((currentPeak <= mindB + 0.01f) ? String("-inf dB") : String(currentPeak, 1) + " dB");
        float peakTextAreaWidth = getTextLayoutWidth(peakText, peakFont) + 10.f; float peakTextAreaHeight = 15.f;
        juce::Rectangle<float> peakTextArea(graphBounds.getRight() - peakTextAreaWidth, graphBounds.getY(), peakTextAreaWidth, peakTextAreaHeight);
        g.drawText(peakText, peakTextArea.toNearestInt(), Justification::centredRight, false);

        g.setColour(Colours::darkgrey);
        g.drawRect(getLocalBounds(), 1.f);
    }

    //==============================================================================
    void SpectrumAnalyzer::resized()
    {
        // ������������� �������� ��������� �����, ��� �������� ����� � timerCallback
        static constexpr int framesToWaitBeforePaintingAfterResizing = 5;
        resizeDebounceInFrames = framesToWaitBeforePaintingAfterResizing;
        DBG("Resized called. Debounce set.");
    }

    //==============================================================================
    // --- ������� ��������� ����� � ���� ---
    void SpectrumAnalyzer::drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto width = bounds.getWidth();

        float freqs[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
                          2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
        g.setColour(gridLineColour);
        for (float f : freqs) {
            float x = left + frequencyToX(f, width);
            if (x >= left && x <= right) g.drawVerticalLine(roundToInt(x), top, bottom);
        }

        float labelFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        g.setColour(gridTextColour);
        auto freqLabelFont = juce::Font(juce::FontOptions(9.f)); g.setFont(freqLabelFont);
        for (float f : labelFreqs) {
            float x = left + frequencyToX(f, width);
            String str = (f >= 1000.f) ? String(f / 1000.f, (f < 10000.f ? 1 : 0)) + "k" : String(roundToInt(f));
            float textW = getTextLayoutWidth(str, freqLabelFont);
            Rectangle<float> r(0, 0, textW, 10.f); r.setCentre(x, bottom - 5.f);
            if (r.getX() >= left - 2.f && r.getRight() <= right + 2.f) g.drawFittedText(str, r.toNearestInt(), Justification::centred, 1);
        }
    }

    void SpectrumAnalyzer::drawGainScale(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto top = bounds.getY(); auto bottom = bounds.getBottom();

        std::vector<float> levels; for (float db = maxdB; db >= mindB; db -= 6.0f) levels.push_back(db); levels.push_back(0.0f);
        std::sort(levels.begin(), levels.end()); levels.erase(std::unique(levels.begin(), levels.end(), [](float a, float b) { return approximatelyEqual(a, b); }), levels.end());
        auto labelFont = Font(FontOptions(9.f)); g.setFont(labelFont);

        for (float db : levels) {
            float y = jlimit(top, jmap(db, mindB, maxdB, bottom, top), bottom);
            bool isZero = approximatelyEqual(db, 0.0f); bool isMajor = isZero || (roundToInt(db) != 0 && roundToInt(db) % 12 == 0);
            g.setColour(isZero ? zeroDbLineColour : (isMajor ? gridLineColour.brighter(0.2f) : gridLineColour));
            g.drawHorizontalLine(roundToInt(y), left, right);
            if (isMajor || db == maxdB || db == mindB) {
                g.setColour(gridTextColour); String str = String(roundToInt(db)); if (db > 0.01f && !str.startsWithChar('+')) str = "+" + str;
                float textW = getTextLayoutWidth(str, labelFont);
                g.drawText(str, roundToInt(left + 4.f), roundToInt(y - 5.f), roundToInt(textW), 10, Justification::centredLeft);
                g.drawText(str, roundToInt(right - textW - 4.f), roundToInt(y - 5.f), roundToInt(textW), 10, Justification::centredRight);
            }
        }
    }

    // --- ������� ��������� ������� � ����� ---
    void SpectrumAnalyzer::drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto width = bounds.getWidth(); auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto numBins = displayData.size(); auto sampleRate = processor.getSampleRate();
        if (numBins == 0 || peakHoldLevels.size() != numBins || sampleRate <= 0 || width <= 0) return;

        std::vector<Point<float>> spectrumPoints; std::vector<Point<float>> peakPointsVec;
        spectrumPoints.reserve(numBins); peakPointsVec.reserve(numBins);
        const int firstBinToDraw = 2; const float lowFreqRollOffEndBin = 10.0f;

        spectrumPoints.push_back({ left, bottom }); peakPointsVec.push_back({ left, bottom });

        for (size_t i = firstBinToDraw; i < numBins; ++i) {
            float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);
            if (freq >= minFreq && freq <= maxFreq) {
                float x = left + frequencyToX(freq, width); x = jlimit(left, right, x);
                float displayDb = displayData[i]; float peakDb = peakHoldLevels[i];
                float lowFreqAttenuation = 1.0f;
                if (i < firstBinToDraw + lowFreqRollOffEndBin) {
                    lowFreqAttenuation = jmap(float(i), float(firstBinToDraw - 1), float(firstBinToDraw + lowFreqRollOffEndBin), 0.3f, 1.0f);
                    displayDb = mindB + (displayDb - mindB) * lowFreqAttenuation;
                    peakDb = mindB + (peakDb - mindB) * lowFreqAttenuation;
                }
                float yDisplay = jlimit(top, jmap(displayDb, mindB, maxdB, bottom, top), bottom);
                float yPeak = jlimit(top, jmap(peakDb, mindB, maxdB, bottom, top), bottom);

                spectrumPoints.push_back({ x, yDisplay });
                // ����������� ��� ������� ����� (����� �� ���� "�������" �� ����������)
                if (peakPointsVec.empty() || !juce::approximatelyEqual(x, peakPointsVec.back().x)) {
                    peakPointsVec.push_back({ x, yPeak });
                }
                else {
                    peakPointsVec.back().y = std::min(peakPointsVec.back().y, yPeak);
                }
            }
            if (freq > maxFreq * 1.05) break;
        }
        spectrumPoints.push_back({ right, bottom }); peakPointsVec.push_back({ right, bottom });

        if (spectrumPoints.size() < 2) return;

        // ������ �������� ������ (EMA) � cubicTo
        Path spectrumPath; spectrumPath.startNewSubPath(spectrumPoints[0]);
        for (size_t i = 1; i < spectrumPoints.size(); ++i) {
            const auto& p0 = spectrumPoints[i - 1]; const auto& p1 = spectrumPoints[i];
            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
            spectrumPath.cubicTo(cp1, cp2, p1);
        }
        g.setColour(spectrumFillColour); g.fillPath(spectrumPath);
        g.setColour(spectrumLineColour); g.strokePath(spectrumPath, PathStrokeType(1.5f));

        // ������ ������� ������ � lineTo
        if (peakPointsVec.size() >= 2) {
            Path peakPath; peakPath.startNewSubPath(peakPointsVec[0]);
            for (size_t i = 1; i < peakPointsVec.size(); ++i) peakPath.lineTo(peakPointsVec[i]);
            g.setColour(peakHoldLineColour); g.strokePath(peakPath, PathStrokeType(1.0f));
        }

        // ������ ������� ������ > 0 �� (��� �������� ����� displayData, � cubicTo)
        Path overZeroPath; bool pathStarted = false;
        float zeroDbY = jlimit(top, jmap(0.0f, mindB, maxdB, bottom, top), bottom);
        std::vector<Point<float>> pointsAboveZero;

        for (size_t i = firstBinToDraw; i < numBins; ++i)
        {
            float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);
            if (freq < minFreq || freq > maxFreq) continue;

            float originalDb = displayData[i]; // �������� ��������
            float displayDb = originalDb; // �������� ��� ��������� � ��������
            float x = left + frequencyToX(freq, width);
            x = jlimit(left, right, x);

            // ��������� ���������� �� � �������� ��� ���������/��������
            float lowFreqAttenuation = 1.0f;
            if (i < firstBinToDraw + lowFreqRollOffEndBin) {
                lowFreqAttenuation = juce::jmap(float(i), float(firstBinToDraw - 1), float(firstBinToDraw + lowFreqRollOffEndBin), 0.3f, 1.0f);
                displayDb = mindB + (originalDb - mindB) * lowFreqAttenuation;
            }

            // --- ��������: ��������� ����������� �������� displayDb ---
            if (displayDb >= 0.0f)
            {
                // ������ ����������� �������� � Y
                float y = jlimit(top, jmap(displayDb, mindB, maxdB, bottom, top), bottom);

                if (!pathStarted)
                {
                    // --- ������ ���������� ����� ---
                    // ����� ���� ����� ������������ ����������� �������� prevDisplayDb ��� ���������������
                    size_t prev_i = (i > firstBinToDraw) ? i - 1 : i;
                    float prevOriginalDb = (i > firstBinToDraw) ? displayData[prev_i] : mindB;
                    float prevDisplayDb = prevOriginalDb; // �������� � �������������
                    // ��������� prevDb, ���� �� � ���������
                    if (prev_i >= firstBinToDraw && prev_i < firstBinToDraw + lowFreqRollOffEndBin) {
                        float prevAttenuation = juce::jmap(float(prev_i), float(firstBinToDraw - 1), float(firstBinToDraw + lowFreqRollOffEndBin), 0.3f, 1.0f);
                        prevDisplayDb = mindB + (prevOriginalDb - mindB) * prevAttenuation;
                    }
                    // --- ����� ���������� prevDb ---

                    if (prevDisplayDb < 0.0f) // ��������� ����������� ����������
                    {
                        float prevFreq = static_cast<float>(prev_i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);
                        if (prevFreq >= minFreq) {
                            float prevX = jlimit(left, right, left + frequencyToX(prevFreq, width));
                            pointsAboveZero.push_back({ prevX, zeroDbY });
                        }
                        else {
                            pointsAboveZero.push_back({ left, zeroDbY });
                        }
                    }
                    pointsAboveZero.push_back({ x, y });
                    pathStarted = true;
                }
                else // ���������� �������
                {
                    pointsAboveZero.push_back({ x, y });
                }
            }
            else // ����������� �������� ���� 0 ��
            {
                if (pathStarted) // ����������� �������
                {
                    pointsAboveZero.push_back({ x, zeroDbY });
                    if (pointsAboveZero.size() >= 2)
                    {
                        // ������ � ������ overZeroPath ��� ������
                        overZeroPath.startNewSubPath(pointsAboveZero[0]);
                        for (size_t p_idx = 1; p_idx < pointsAboveZero.size(); ++p_idx) {
                            // ������ ���� ��� �������� � cubicTo
                            overZeroPath.startNewSubPath(pointsAboveZero[0]);
                            for (size_t p_idx = 1; p_idx < pointsAboveZero.size(); ++p_idx)
                            {
                                const auto& p0 = pointsAboveZero[p_idx - 1];
                                const auto& p1 = pointsAboveZero[p_idx];
                                // ������������ ����������� ����� ��� �������� ��������
                                juce::Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y };
                                juce::Point<float> cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                                overZeroPath.cubicTo(cp1, cp2, p1);
                            }
                            // ������ ����
                            g.setColour(overZeroDbLineColour); // ������� ����
                            g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
                        }
                        g.setColour(overZeroDbLineColour);
                        g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
                    }
                    overZeroPath.clear(); pointsAboveZero.clear(); pathStarted = false;
                }
            }
        }
        // ��������� ��������� �������
        if (pathStarted && pointsAboveZero.size() >= 2)
        {
            // ������ � ������ overZeroPath ��� ������
            overZeroPath.startNewSubPath(pointsAboveZero[0]);
            for (size_t p_idx = 1; p_idx < pointsAboveZero.size(); ++p_idx) {
                overZeroPath.startNewSubPath(pointsAboveZero[0]);
                for (size_t p_idx = 1; p_idx < pointsAboveZero.size(); ++p_idx)
                {
                    const auto& p0 = pointsAboveZero[p_idx - 1];
                    const auto& p1 = pointsAboveZero[p_idx];
                    // ������������ ����������� �����
                    juce::Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y };
                    juce::Point<float> cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                    overZeroPath.cubicTo(cp1, cp2, p1);
                }
                // ������ ����
                g.setColour(overZeroDbLineColour); // ������� ����
                g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
            }
            g.setColour(overZeroDbLineColour);
            g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
        }
    }

    // --- ��������� �������� ���������� ---
    void SpectrumAnalyzer::drawFrequencyMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto width = bounds.getWidth(); auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto left = bounds.getX(); auto right = bounds.getRight();

        float lowFreq = processor.lowMidCrossover ? processor.lowMidCrossover->get() : 20.0f;
        float highFreq = processor.midHighCrossover ? processor.midHighCrossover->get() : 20000.0f;
        Colour lowColour = Colours::orange; Colour highColour = Colours::cyan;

        auto drawMarker = [&](float freq, Colour colour) {
            float x = left + frequencyToX(freq, width);
            if (x >= left && x <= right) {
                g.setColour(colour.withAlpha(0.7f)); g.drawVerticalLine(roundToInt(x), top, bottom);
                g.setColour(colour); g.fillEllipse(x - 2.f, top, 4.f, 4.f);
            }
            };
        drawMarker(lowFreq, lowColour); drawMarker(highFreq, highColour);
    }

    // --- �������������� ������� � X ---
    float SpectrumAnalyzer::frequencyToX(float freq, float width) const
    {
        freq = juce::jlimit(minFreq, maxFreq, freq);
        if (maxFreq / minFreq <= 1.0f || freq <= 0 || width <= 0) return 0.0f;
        return width * (std::log(freq / minFreq) / std::log(maxFreq / minFreq));
    }

} // namespace MBRP_GUI