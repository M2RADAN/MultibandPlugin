#include "SpectrumAnalyzer.h" 
#include <vector>
#include <cmath>      
#include <limits>     
#include <algorithm>  
#include <juce_gui_basics/juce_gui_basics.h> 
#include <juce_dsp/juce_dsp.h>  


namespace MBRP_GUI
{


    /*static*/float SpectrumAnalyzer::getTextLayoutWidth(const juce::String& text, const juce::Font& font)
    {
        juce::TextLayout textLayout;
        juce::AttributedString attrString(text);
        attrString.setFont(font);
        textLayout.createLayout(attrString, 10000.0f); // Use a large width to get the natural width
        return textLayout.getWidth();
    }


    SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& p) :
        processor{ p },

        displayData(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        peakHoldLevels(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        fftPoints(MBRPAudioProcessor::fftSize)
    {
        peakDbLevel.store(mindB);   
        avgInput.clear();          
        avgOutput.clear();
        startTimerHz(60);        
    }

    //==============================================================================

    void SpectrumAnalyzer::timerCallback()
    {
        bool newDataAvailable = processor.nextFFTBlockReady.load();

        if (newDataAvailable)
        {
            drawNextFrame();
            processor.nextFFTBlockReady.store(false); 
        }


        if (resizeDebounceInFrames > 0)
        {
            --resizeDebounceInFrames;
            if (resizeDebounceInFrames == 0)
            {
                recalculateFftPoints();

                if (!newDataAvailable) repaint();
            }

            return;
        }

        bool peakNeedsRepaint = false;
        for (size_t i = 0; i < peakHoldLevels.size(); ++i)
        {
            float oldPeakDb = peakHoldLevels[i];

            if (oldPeakDb > displayData[i] && oldPeakDb > mindB + 0.01f)
            {
                peakHoldLevels[i] = std::max(displayData[i], juce::Decibels::gainToDecibels(juce::Decibels::decibelsToGain(oldPeakDb) * peakHoldDecayFactor, mindB));
                if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) {
                    peakNeedsRepaint = true; 
                }
            }
            else if (oldPeakDb > mindB + 0.01f && peakHoldLevels[i] < displayData[i]) {

                peakHoldLevels[i] = displayData[i];
                if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) {
                    peakNeedsRepaint = true;
                }
            }
        }


        if (newDataAvailable || peakNeedsRepaint) {
            repaint();
        }
    }



    void SpectrumAnalyzer::drawNextFrame()
    {
        bool newDataProcessed = false; 


        while (processor.abstractFifoInput.getNumReady() >= MBRPAudioProcessor::fftSize)
        {
            fftBufferInput.clear(); 
            int start1, block1, start2, block2;

            processor.abstractFifoInput.prepareToRead(MBRPAudioProcessor::fftSize, start1, block1, start2, block2);


            const int audioFifoSize = processor.audioFifoInput.getNumSamples();
            if (audioFifoSize > 0) {
                if (block1 > 0) fftBufferInput.copyFrom(0, 0, processor.audioFifoInput.getReadPointer(0, start1 % audioFifoSize), block1);
                if (block2 > 0) fftBufferInput.copyFrom(0, block1, processor.audioFifoInput.getReadPointer(0, start2 % audioFifoSize), block2);
            }
            // --- ---


            processor.abstractFifoInput.finishedRead(block1 + block2);


            hannWindow.multiplyWithWindowingTable(fftBufferInput.getWritePointer(0), static_cast<size_t>(MBRPAudioProcessor::fftSize));

            fftInput.performFrequencyOnlyForwardTransform(fftBufferInput.getWritePointer(0));

            {
                juce::ScopedLock lockedForAvgUpdate(pathCreationLock);

                avgInput.addFrom(0, 0, avgInput.getReadPointer(avgInputPtr), avgInput.getNumSamples(), -1.0f);

                float normFactor = 1.0f / (static_cast<float>(avgInput.getNumChannels() - 1));
                avgInput.copyFrom(avgInputPtr, 0, fftBufferInput.getReadPointer(0), avgInput.getNumSamples(), normFactor);

                avgInput.addFrom(0, 0, avgInput.getReadPointer(avgInputPtr), avgInput.getNumSamples());
            }

            if (++avgInputPtr >= avgInput.getNumChannels()) avgInputPtr = 1;

            newDataProcessed = true;
        }





        if (newDataProcessed)
        {
            auto numBins = avgInput.getNumSamples();
            std::vector<float> latestDbData(numBins);

            if (displayData.size() != numBins) displayData.resize(numBins);
            if (peakHoldLevels.size() != numBins) peakHoldLevels.resize(numBins);


            const float* averagedMagnitudes = nullptr;
            {
                juce::ScopedLock lockedForAvgRead(pathCreationLock);
                if (avgInput.getNumSamples() > 0) averagedMagnitudes = avgInput.getReadPointer(0);
            }
            if (averagedMagnitudes == nullptr) return; 


            
            const float gainMultiplier = juce::Decibels::decibelsToGain(gainAdjustment);
            // -------------------------------------------

            float currentFramePeak = mindB;

            for (size_t i = 0; i < numBins; ++i)
            {
                float finalMagnitude = averagedMagnitudes[i] * gainMultiplier;
                latestDbData[i] = juce::Decibels::gainToDecibels(finalMagnitude, mindB);
                if (latestDbData[i] > currentFramePeak) currentFramePeak = latestDbData[i];
            }
            peakDbLevel.store(currentFramePeak); 


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

        }
    }

    //==============================================================================

    void SpectrumAnalyzer::recalculateFftPoints()
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto width = bounds.getWidth();
        if (width <= 0) { fftPointsSize = 0; return; }

        const auto sampleRate = static_cast<float> (processor.getSampleRate());
        const auto fftSizeHalved = static_cast<int> (MBRPAudioProcessor::fftSize / 2);
        if (sampleRate <= 0 || fftSizeHalved <= 0) { fftPointsSize = 0; return; } 

        const float minLogFreq = 20.0f;
        const float maxLogFreq = sampleRate / 2.0f;
        if (maxLogFreq <= minLogFreq) { fftPointsSize = 0; return; }
        const float logRange = std::log(maxLogFreq / minLogFreq);

        if (fftPoints.size() != MBRPAudioProcessor::fftSize) 
            fftPoints.resize(MBRPAudioProcessor::fftSize);

        fftPointsSize = 0;
        int lastX = -1;

        for (int i = 1; i < fftSizeHalved; ++i) 
        {
            const float freq = sampleRate * static_cast<float>(i) / static_cast<float>(MBRPAudioProcessor::fftSize);
            if (freq < minLogFreq) continue;

            const float logPos = std::log(freq / minLogFreq) / logRange;
            const int x = juce::roundToInt(logPos * width);

            if (x >= width) break;

            if (x > lastX)
            {
                if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i - 1;
                if (fftPointsSize < (int)fftPoints.size()) {
                    fftPoints[fftPointsSize].firstBinIndex = i;
                    fftPoints[fftPointsSize].x = x;
                    fftPointsSize++;
                    lastX = x;
                }
                else { jassertfalse; break; }
            }
            if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i; 
        }
        if (fftPointsSize > 0) { 
            fftPoints[fftPointsSize - 1].lastBinIndex = std::min(fftPoints[fftPointsSize - 1].lastBinIndex, fftSizeHalved - 1);
        }
        DBG("Recalculated FFT points. Count: " << fftPointsSize << " for width: " << width);
    }

    //==============================================================================

    /*static*/ float SpectrumAnalyzer::getFftPointLevel(const float* averagedMagnitudes, const fftPoint& point)
    {
        float maxMagnitude = 0.0f;
        const int numSamples = MBRPAudioProcessor::fftSize / 2;

        for (int i = point.firstBinIndex; i <= point.lastBinIndex; ++i) {
            if (i >= 0 && i < numSamples) {
                if (averagedMagnitudes[i] > maxMagnitude) maxMagnitude = averagedMagnitudes[i];
            }
        }

        return juce::Decibels::gainToDecibels(maxMagnitude, mindB); 

    }

    //==============================================================================


    void SpectrumAnalyzer::setAnalyzerActive(bool isActive)
    {
        if (isActive != analyzerIsActive.load())
        {
            analyzerIsActive.store(isActive);
            if (!isActive) {
                // Очистка буферов (оставляем опционально)
                // displayData.assign(displayData.size(), mindB);
                // peakHoldLevels.assign(peakHoldLevels.size(), mindB);
            }
            // setVisible(isActive); // <-- ЗАКОММЕНТИРОВАТЬ ИЛИ УДАЛИТЬ
            repaint();
        }
    }



    void SpectrumAnalyzer::paint(juce::Graphics& g)
    {
        using namespace juce;
        g.fillAll(ColorScheme::getAnalyzerBackgroundColor()); // Заливаем фон в любом случае
        auto bounds = getLocalBounds().toFloat();
        auto graphBounds = bounds.reduced(1.f, 5.f); // Получаем область графика

        // Пересчитываем точки, если нужно
        if (fftPointsSize == 0 && getWidth() > 0) {
            recalculateFftPoints();
        }

        // Рисуем сетку и шкалу ВСЕГДА
        drawFrequencyGrid(g, graphBounds);
        drawGainScale(g, graphBounds);

        // --- Рисуем спектр и пики ТОЛЬКО если анализатор активен ---
        if (analyzerIsActive.load()) // Проверяем флаг
        {
            drawSpectrumAndPeaks(g, graphBounds);

            // Рисуем текст пикового значения только если активен
            g.setColour(ColorScheme::getAnalyzerPeakTextColor());
            auto peakFont = juce::Font(juce::FontOptions(12.0f)); g.setFont(peakFont);
            float currentPeak = peakDbLevel.load();
            String peakText = "Peak: " + ((currentPeak <= mindB + 0.01f) ? String("-inf dB") : String(currentPeak, 1) + " dB");
            float peakTextAreaWidth = getTextLayoutWidth(peakText, peakFont) + 10.f; float peakTextAreaHeight = 15.f;
            juce::Rectangle<float> peakTextArea(graphBounds.getRight() - peakTextAreaWidth, graphBounds.getY(), peakTextAreaWidth, peakTextAreaHeight);
            g.drawText(peakText, peakTextArea.toNearestInt(), Justification::centredRight, false);
        }
        else
        {
            // Если неактивен, можно нарисовать полупрозрачную заглушку поверх сетки
            // или просто оставить сетку видимой без спектра.
            // Пример с заглушкой:
            // g.setColour(ColorScheme::getAnalyzerBackgroundColor().withAlpha(0.7f)); // Полупрозрачный цвет фона
            // g.fillRect(graphBounds); // Заливаем только область графика
            // g.setColour(ColorScheme::getScaleTextColor().withAlpha(0.5f));
            // g.setFont(15.0f);
            // g.drawFittedText("Analyzer Disabled", graphBounds.toNearestInt(), juce::Justification::centred, 1);

            // Или просто ничего не рисуем поверх сетки, если хотим оставить ее чистой.
        }
        // -----------------------------------------------------------

        // Рисуем рамку компонента всегда
        g.setColour(juce::Colours::lightgrey); // Или ColorScheme::getComponentOutlineColor()
        g.drawRect(getLocalBounds(), 1.f);
    }

    //==============================================================================
    void SpectrumAnalyzer::resized()
    {

        static constexpr int framesToWaitBeforePaintingAfterResizing = 5;
        resizeDebounceInFrames = framesToWaitBeforePaintingAfterResizing;
        //DBG("Resized called. Debounce set.");
    }

    //==============================================================================

    void SpectrumAnalyzer::drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto width = bounds.getWidth();

        float freqs[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
                          2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
        g.setColour(ColorScheme::getAnalyzerGridBaseColor().withAlpha(0.3f));
        for (float f : freqs) {
            float x = left + frequencyToX(f, width);
            if (x >= left && x <= right) g.drawVerticalLine(roundToInt(x), top, bottom);
        }

        float labelFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        g.setColour(ColorScheme::getScaleTextColor());
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
        //coulours
        const auto zeroLineCol = ColorScheme::getZeroDbLineBaseColor();
        const auto gridLineCol = ColorScheme::getAnalyzerGridBaseColor();
        const auto gridTextCol = ColorScheme::getScaleTextColor(); // Цвет текста шкалы

        for (float db : levels) {
            float y = jlimit(top, jmap(db, mindB, maxdB, bottom, top), bottom);
            bool isZero = approximatelyEqual(db, 0.0f); bool isMajor = isZero || (roundToInt(db) != 0 && roundToInt(db) % 12 == 0);

            g.setColour(isZero ? zeroLineCol.withAlpha(0.5f) : (isMajor ? gridLineCol.brighter(0.2f).withAlpha(0.4f) : gridLineCol.withAlpha(0.3f)));
            g.drawHorizontalLine(roundToInt(y), left, right);

            if (isMajor || db == maxdB || db == mindB) {
                g.setColour(gridTextCol); String str = String(roundToInt(db)); if (db > 0.01f && !str.startsWithChar('+')) str = "+" + str;
                float textW = getTextLayoutWidth(str, labelFont);
                g.drawText(str, roundToInt(left + 4.f), roundToInt(y - 5.f), roundToInt(textW), 10, Justification::centredLeft);
                g.drawText(str, roundToInt(right - textW - 4.f), roundToInt(y - 5.f), roundToInt(textW), 10, Justification::centredRight);
            }
        }
    }


    void SpectrumAnalyzer::drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto width = bounds.getWidth(); auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto numBins = displayData.size(); 
        auto sampleRate = processor.getSampleRate();


        if (numBins == 0 || peakHoldLevels.size() != numBins || sampleRate <= 0 || width <= 0) return;


        std::vector<Point<float>> spectrumPoints;
        std::vector<Point<float>> peakPointsVec;
        spectrumPoints.reserve(numBins);
        peakPointsVec.reserve(numBins);


        const int firstBinToDraw = 2;           
        const float lowFreqRollOffEndBin = 5.0f;  


        spectrumPoints.push_back({ left, bottom });
        peakPointsVec.push_back({ left, bottom });
        //colour
        const auto spectrumFillBaseCol = ColorScheme::getSpectrumFillBaseColor();
        const auto spectrumLineCol = ColorScheme::getSpectrumLineColor();
        const auto peakHoldLineBaseCol = ColorScheme::getPeakHoldLineBaseColor();
        const auto overZeroLineCol = ColorScheme::getOverZeroDbLineColor();

        for (size_t i = firstBinToDraw; i < numBins; ++i)
        {
            float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);


            if (freq >= minFreq && freq <= maxFreq)
            {
                float x = left + frequencyToX(freq, width);
                x = jlimit(left, right, x); 

                float displayDb = displayData[i]; 
                float peakDb = peakHoldLevels[i];


                float lowFreqAttenuation = 1.0f;
                if (i < firstBinToDraw + lowFreqRollOffEndBin) {
                    lowFreqAttenuation = juce::jmap(float(i), float(firstBinToDraw - 1), float(firstBinToDraw + lowFreqRollOffEndBin), 0.45f, 1.0f); 

                    displayDb = mindB + (displayDb - mindB) * lowFreqAttenuation;
                    peakDb = mindB + (peakDb - mindB) * lowFreqAttenuation;
                }



                float yDisplay = jlimit(top, jmap(displayDb, mindB, maxdB, bottom, top), bottom);
                float yPeak = jlimit(top, jmap(peakDb, mindB, maxdB, bottom, top), bottom);

                spectrumPoints.push_back({ x, yDisplay });


                if (peakPointsVec.empty() || !juce::approximatelyEqual(x, peakPointsVec.back().x)) {
                    peakPointsVec.push_back({ x, yPeak }); 
                }
                else {

                    peakPointsVec.back().y = std::min(peakPointsVec.back().y, yPeak);
                }
            }

            if (freq > maxFreq * 1.05) break;
        }

        spectrumPoints.push_back({ right, bottom });
        peakPointsVec.push_back({ right, bottom });

        if (spectrumPoints.size() < 2) return;


        Path spectrumPath;
        spectrumPath.startNewSubPath(spectrumPoints[0]);
        for (size_t i = 1; i < spectrumPoints.size(); ++i) {
            const auto& p0 = spectrumPoints[i - 1];
            const auto& p1 = spectrumPoints[i];

            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y };
            Point<float> cp2{ (p0.x + p1.x) * 0.5f, p1.y };
            spectrumPath.cubicTo(cp1, cp2, p1);
        }

        // Заливка с альфой
        g.setColour(spectrumFillBaseCol.withAlpha(0.2f));
        g.fillPath(spectrumPath);

        // Линия спектра
        g.setColour(spectrumLineCol);
        g.strokePath(spectrumPath, PathStrokeType(1.5f));



        if (peakPointsVec.size() >= 2) {
            Path peakPath;
            peakPath.startNewSubPath(peakPointsVec[0]);
            for (size_t i = 1; i < peakPointsVec.size(); ++i) {
                peakPath.lineTo(peakPointsVec[i]);
            }
            g.setColour(peakHoldLineBaseCol.withAlpha(0.7f));
            g.strokePath(peakPath, PathStrokeType(1.0f)); 
        }


        Path overZeroPath;
        bool isCurrentlyAboveZero = false;
        float zeroDbY = jlimit(top, jmap(0.0f, mindB, maxdB, bottom, top), bottom); 

        std::vector<Point<float>> currentSegmentPoints;


        for (size_t i = 1; i < spectrumPoints.size() - 1; ++i) 
        {
            const auto& currentPoint = spectrumPoints[i];
            const float y = currentPoint.y;
            const float x = currentPoint.x;


            if (y <= zeroDbY) 
            {
                if (!isCurrentlyAboveZero) 
                {
                    isCurrentlyAboveZero = true;
                    currentSegmentPoints.clear();
                    const auto& prevPoint = spectrumPoints[i - 1];


                    if (prevPoint.y > zeroDbY) {

                        float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                        intersectX = jlimit(left, intersectX, right);
                        currentSegmentPoints.push_back({ intersectX, zeroDbY });
                    }
                    else {

                        currentSegmentPoints.push_back(prevPoint);
                    }
                    currentSegmentPoints.push_back(currentPoint);
                }
                else 
                {
                    currentSegmentPoints.push_back(currentPoint);
                }
            }
            else
            {
                if (isCurrentlyAboveZero)
                {
                    isCurrentlyAboveZero = false;
                    const auto& prevPoint = spectrumPoints[i - 1];


                    float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                    intersectX = jlimit(left, intersectX, right);
                    currentSegmentPoints.push_back({ intersectX, zeroDbY }); 


                    if (currentSegmentPoints.size() >= 2) {
                        overZeroPath.startNewSubPath(currentSegmentPoints[0]);
                        for (size_t p_idx = 1; p_idx < currentSegmentPoints.size(); ++p_idx) {
                            const auto& p0 = currentSegmentPoints[p_idx - 1]; const auto& p1 = currentSegmentPoints[p_idx];
                            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                            overZeroPath.cubicTo(cp1, cp2, p1);
                        }
                    }
                }

            }
        }


        if (isCurrentlyAboveZero && currentSegmentPoints.size() >= 2)
        {

            overZeroPath.startNewSubPath(currentSegmentPoints[0]);
            for (size_t p_idx = 1; p_idx < currentSegmentPoints.size(); ++p_idx) {
                const auto& p0 = currentSegmentPoints[p_idx - 1]; const auto& p1 = currentSegmentPoints[p_idx];
                Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                overZeroPath.cubicTo(cp1, cp2, p1);
            }
        }


        if (!overZeroPath.isEmpty()) {
            g.setColour(overZeroLineCol);
            g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
        }

    }

    //void SpectrumAnalyzer::drawFrequencyMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    //{
    //    using namespace juce;
    //    auto width = bounds.getWidth(); auto top = bounds.getY(); auto bottom = bounds.getBottom();
    //    auto left = bounds.getX(); auto right = bounds.getRight();

    //    float lowFreq = processor.lowMidCrossover ? processor.lowMidCrossover->get() : 20.0f;
    //    float highFreq = processor.midHighCrossover ? processor.midHighCrossover->get() : 20000.0f;
    //    Colour lowColour = Colours::orange; Colour highColour = Colours::cyan;

    //    auto drawMarker = [&](float freq, Colour colour) {
    //        float x = left + frequencyToX(freq, width);
    //        if (x >= left && x <= right) {
    //            g.setColour(colour.withAlpha(0.7f)); g.drawVerticalLine(roundToInt(x), top, bottom);
    //            g.setColour(colour); g.fillEllipse(x - 2.f, top, 4.f, 4.f);
    //        }
    //        };
    //    drawMarker(lowFreq, lowColour); drawMarker(highFreq, highColour);
    //}

    float SpectrumAnalyzer::frequencyToX(float freq, float width) const
    {
        freq = juce::jlimit(minFreq, maxFreq, freq);
        if (maxFreq / minFreq <= 1.0f || freq <= 0 || width <= 0) return 0.0f;
        return width * (std::log(freq / minFreq) / std::log(maxFreq / minFreq));
    }

}
