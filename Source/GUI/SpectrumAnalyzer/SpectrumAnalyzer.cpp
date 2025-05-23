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
        textLayout.createLayout(attrString, 10000.0f);
        return textLayout.getWidth();
    }

    SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& p) :
        processor{ p },
        displayData(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        peakHoldLevels(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        fftPoints(MBRPAudioProcessor::fftSize)
    {
        peakDbLevel.store(mindB);
        avgSpectrumData.clear(); // Используем avgSpectrumData (бывший avgInput)        
        startTimerHz(60);
    }

    void SpectrumAnalyzer::timerCallback()
    {
        bool newDataAvailableForProcessing = false; // Локальный флаг для этого вызова

        if (processor.nextFFTBlockReady.load()) // Проверяем общий флаг из процессора
        {
            // Проверяем, есть ли данные именно в ВЫХОДНОМ FIFO
            if (processor.abstractFifoOutput.getNumReady() >= MBRPAudioProcessor::fftSize) {
                drawNextFrame(); // Эта функция сама сбросит nextFFTBlockReady, если обработает данные
                newDataAvailableForProcessing = processor.nextFFTBlockReady.load() == false; // true, если drawNextFrame обработал и сбросил
            }
        }

        if (resizeDebounceInFrames > 0)
        {
            --resizeDebounceInFrames;
            if (resizeDebounceInFrames == 0)
            {
                recalculateFftPoints();
                // Если не было новых данных для ОБРАБОТКИ в этом вызове, но был ресайз, 
                // перерисовываем с текущими displayData
                if (!newDataAvailableForProcessing) {
                    repaint();
                }
            }
            // Не выходим здесь, чтобы пики могли обновиться
        }

        bool peakNeedsRepaint = false;
        if (analyzerIsActive.load()) { // Обновляем пики только если активен
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
                else if (displayData[i] > oldPeakDb) { // Если текущий уровень спектра выше пика
                    peakHoldLevels[i] = displayData[i];
                    if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) {
                        peakNeedsRepaint = true;
                    }
                }
            }
        }


        // Перерисовываем, если были обработаны новые данные ИЛИ если пики требуют обновления
        if (newDataAvailableForProcessing || peakNeedsRepaint) {
            repaint();
        }
    }

    void SpectrumAnalyzer::drawNextFrame()
    {
        bool dataWasProcessedInThisLoopIteration = false;

        // --- Используем ВЫХОДНОЙ FIFO процессора ---
        while (processor.abstractFifoOutput.getNumReady() >= MBRPAudioProcessor::fftSize)
        {
            fftBuffer.clear();
            int start1, block1, start2, block2;

            processor.abstractFifoOutput.prepareToRead(MBRPAudioProcessor::fftSize, start1, block1, start2, block2);

            const int audioFifoSize = processor.audioFifoOutput.getNumSamples();
            if (audioFifoSize > 0) { // Убедимся, что буфер FIFO не пуст (хотя getNumReady уже это проверяет)
                if (block1 > 0) fftBuffer.copyFrom(0, 0, processor.audioFifoOutput.getReadPointer(0, start1 % audioFifoSize), block1);
                if (block2 > 0) fftBuffer.copyFrom(0, block1, processor.audioFifoOutput.getReadPointer(0, start2 % audioFifoSize), block2);
            }

            processor.abstractFifoOutput.finishedRead(block1 + block2);
            // --- Конец чтения из ВЫХОДНОГО FIFO ---

            // Применяем окно Ханна
            hannWindow.multiplyWithWindowingTable(fftBuffer.getWritePointer(0), static_cast<size_t>(MBRPAudioProcessor::fftSize));

            // Выполняем БПФ
            forwardFFT.performFrequencyOnlyForwardTransform(fftBuffer.getWritePointer(0)); // Результат в fftBuffer

            // Усреднение магнитуд (алгоритм как был для avgInput)
            // avgSpectrumData - это AudioBuffer<float> { numAvgFrames, fftSize / 2 }
            // fftBuffer - это AudioBuffer<float> { 1, fftSize * 2 }, магнитуды в первой половине
            {
                juce::ScopedLock lockedForAvgUpdate(pathCreationLock);

                // Канал 0 в avgSpectrumData используется для хранения суммы
                // Каналы с 1 по N-1 в avgSpectrumData - это отдельные кадры для усреднения
                const int numAveragingFrames = avgSpectrumData.getNumChannels() - 1;
                if (numAveragingFrames <= 0) { // Защита, если avgSpectrumData настроен некорректно
                    jassertfalse; return;
                }

                // 1. Вычитаем старые значения (из avgSpectrumDataPtr-го канала) из суммы (0-й канал)
                avgSpectrumData.addFrom(0, 0, avgSpectrumData.getReadPointer(avgSpectrumDataPtr), avgSpectrumData.getNumSamples(), -1.0f);

                // 2. Копируем новые магнитуды (из fftBuffer) в avgSpectrumDataPtr-й канал, нормализуя
                float normFactor = 1.0f / static_cast<float>(numAveragingFrames);
                avgSpectrumData.copyFrom(avgSpectrumDataPtr, 0, fftBuffer.getReadPointer(0), avgSpectrumData.getNumSamples(), normFactor);

                // 3. Добавляем новые нормализованные значения (из avgSpectrumDataPtr-го канала) к сумме (0-й канал)
                avgSpectrumData.addFrom(0, 0, avgSpectrumData.getReadPointer(avgSpectrumDataPtr), avgSpectrumData.getNumSamples());
            }

            // Обновляем указатель для циклического буфера усреднения
            if (++avgSpectrumDataPtr >= avgSpectrumData.getNumChannels())
                avgSpectrumDataPtr = 1; // avgSpectrumDataPtr циклически проходит от 1 до N-1

            dataWasProcessedInThisLoopIteration = true;
        }


        if (dataWasProcessedInThisLoopIteration)
        {
            // Сбрасываем флаг готовности данных в процессоре, так как мы их обработали
            processor.nextFFTBlockReady.store(false);

            auto numBins = avgSpectrumData.getNumSamples(); // Это fftSize/2
            std::vector<float> latestDbData(numBins); // Временный вектор для dB значений этого кадра

            // Убедимся, что displayData и peakHoldLevels имеют правильный размер
            if (displayData.size() != numBins) displayData.assign(numBins, mindB);
            if (peakHoldLevels.size() != numBins) peakHoldLevels.assign(numBins, mindB);

            const float* averagedMagnitudes = nullptr;
            {
                juce::ScopedLock lockedForAvgRead(pathCreationLock);
                // Усредненные магнитуды находятся в 0-м канале avgSpectrumData
                if (avgSpectrumData.getNumSamples() > 0)
                    averagedMagnitudes = avgSpectrumData.getReadPointer(0);
            }

            if (averagedMagnitudes == nullptr) {
                DBG("Averaged magnitudes are null!");
                return;
            }

            const float gainMultiplier = juce::Decibels::decibelsToGain(gainAdjustment); // gainAdjustment = 0.0f

            float currentFramePeak = mindB; // Пик для текущего обработанного кадра

            for (size_t i = 0; i < numBins; ++i)
            {
                float finalMagnitude = averagedMagnitudes[i] * gainMultiplier; // Уже усредненная и нормализованная магнитуда
                latestDbData[i] = juce::Decibels::gainToDecibels(finalMagnitude, mindB);
                if (latestDbData[i] > currentFramePeak)
                    currentFramePeak = latestDbData[i];
            }
            peakDbLevel.store(currentFramePeak); // Обновляем общий пик

            // Применяем экспоненциальное сглаживание к displayData и обновляем peakHoldLevels
            for (size_t i = 0; i < numBins; ++i)
            {
                float newValDb = latestDbData[i];       // dB значение текущего усредненного кадра
                float oldDisplayDb = displayData[i];    // Предыдущее отображаемое значение (сглаженное)
                float oldPeakDb = peakHoldLevels[i];    // Предыдущий пик для этого бина

                // Экспоненциальное сглаживание для displayData
                displayData[i] = smoothingAlpha * newValDb + (1.0f - smoothingAlpha) * oldDisplayDb;
                displayData[i] = std::max(mindB, displayData[i]); // Ограничение снизу

                // Обновление пиков
                if (displayData[i] > oldPeakDb) { // Пик обновляется по СГЛАЖЕННОМУ значению (или newValDb, если так нужно)
                    peakHoldLevels[i] = displayData[i]; // Или peakHoldLevels[i] = newValDb; для более резких пиков
                }
                else {
                    // Затухание пика, но не ниже текущего отображаемого уровня
                    peakHoldLevels[i] = std::max(displayData[i],
                        juce::Decibels::gainToDecibels(juce::Decibels::decibelsToGain(oldPeakDb) * peakHoldDecayFactor, mindB));
                }
                peakHoldLevels[i] = std::max(mindB, peakHoldLevels[i]); // Ограничение пика снизу
            }
        }
    }

    void SpectrumAnalyzer::recalculateFftPoints()
    {
        // ... (recalculateFftPoints без изменений) ...
        const auto bounds = getLocalBounds().toFloat();
        const auto width = bounds.getWidth();
        if (width <= 0) { fftPointsSize = 0; return; }
        const auto sampleRate = static_cast<float> (processor.getSampleRate());
        const auto fftSizeHalved = static_cast<int> (MBRPAudioProcessor::fftSize / 2);
        if (sampleRate <= 0 || fftSizeHalved <= 0) { fftPointsSize = 0; return; }
        const float minLogFreq = 20.0f; // Используем константу класса minFreq
        const float maxLogFreq = sampleRate / 2.0f;
        if (maxLogFreq <= minLogFreq) { fftPointsSize = 0; return; }
        const float logRange = std::log(maxLogFreq / minLogFreq);
        if (fftPoints.size() != MBRPAudioProcessor::fftSize) // Убедимся, что вектор имеет достаточный размер
            fftPoints.resize(MBRPAudioProcessor::fftSize);
        fftPointsSize = 0;
        int lastX = -1;
        for (int i = 1; i < fftSizeHalved; ++i)
        {
            const float freq = sampleRate * static_cast<float>(i) / static_cast<float>(MBRPAudioProcessor::fftSize);
            if (freq < minLogFreq) continue; // Пропускаем частоты ниже minLogFreq
            const float logPos = std::log(freq / minLogFreq) / logRange;
            const int x = juce::roundToInt(logPos * width);
            if (x >= width) break; // Выходим, если вышли за пределы ширины
            if (x > lastX) // Если новая X-координата
            {
                if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i - 1;
                // Защита от выхода за пределы fftPoints
                if (fftPointsSize < static_cast<int>(fftPoints.size())) {
                    fftPoints[fftPointsSize].firstBinIndex = i;
                    fftPoints[fftPointsSize].x = x;
                    fftPointsSize++;
                    lastX = x;
                }
                else {
                    // Это не должно происходить, если fftPoints.resize(MBRPAudioProcessor::fftSize)
                    jassertfalse; // Ошибка: пытаемся записать за пределы вектора fftPoints
                    break;
                }
            }
            if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i;
        }
        if (fftPointsSize > 0) { // Убедимся, что последний бин не выходит за пределы
            fftPoints[fftPointsSize - 1].lastBinIndex = std::min(fftPoints[fftPointsSize - 1].lastBinIndex, fftSizeHalved - 1);
        }
    }

    void SpectrumAnalyzer::setAnalyzerActive(bool isActive)
    {
        // ... (setAnalyzerActive без изменений, как в предыдущем ответе) ...
        if (isActive != analyzerIsActive.load())
        {
            analyzerIsActive.store(isActive);
            if (!isActive) {
                std::fill(displayData.begin(), displayData.end(), mindB);
                std::fill(peakHoldLevels.begin(), peakHoldLevels.end(), mindB);
                peakDbLevel.store(mindB);
            }
            repaint();
        }
    }

    void SpectrumAnalyzer::paint(juce::Graphics& g)
    {
        // ... (paint без изменений) ...
        using namespace juce;
        g.fillAll(ColorScheme::getAnalyzerBackgroundColor());
        auto bounds = getLocalBounds().toFloat();
        auto graphBounds = bounds.reduced(1.f, 5.f);
        if ((fftPointsSize == 0 || getWidth() != lastWidthForFftPointsRecalc) && getWidth() > 0) { // Добавил lastWidthForFftPointsRecalc
            recalculateFftPoints();
            lastWidthForFftPointsRecalc = getWidth();
        }
        drawFrequencyGrid(g, graphBounds);
        drawGainScale(g, graphBounds);
        if (analyzerIsActive.load())
        {
            drawSpectrumAndPeaks(g, graphBounds);
            g.setColour(ColorScheme::getAnalyzerPeakTextColor());
            auto peakFont = juce::Font(juce::FontOptions(12.0f)); g.setFont(peakFont);
            float currentPeak = peakDbLevel.load();
            String peakText = "Peak: " + ((currentPeak <= mindB + 0.01f) ? String("-inf dB") : String(currentPeak, 1) + " dB");
            float peakTextAreaWidth = getTextLayoutWidth(peakText, peakFont) + 10.f; float peakTextAreaHeight = 15.f;
            juce::Rectangle<float> peakTextArea(graphBounds.getRight() - peakTextAreaWidth, graphBounds.getY(), peakTextAreaWidth, peakTextAreaHeight);
            g.drawText(peakText, peakTextArea.toNearestInt(), Justification::centredRight, false);
        }
        g.setColour(ColorScheme::getAnalyzerOutlineColor());
        g.drawRect(getLocalBounds(), 1.f);
    }

    // Объявление lastWidthForFftPointsRecalc, если он еще не объявлен в .h
    // int SpectrumAnalyzer::lastWidthForFftPointsRecalc = 0; // Если статический член, но он не статический

    void SpectrumAnalyzer::resized()
    {
        static constexpr int framesToWaitBeforePaintingAfterResizing = 5;
        resizeDebounceInFrames = framesToWaitBeforePaintingAfterResizing;
        // Не вызываем recalculateFftPoints() напрямую здесь, таймер позаботится об этом через debounce.
    }

    void SpectrumAnalyzer::drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        // ... (drawFrequencyGrid без изменений) ...
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
        // ... (drawGainScale без изменений) ...
        using namespace juce;
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto top = bounds.getY(); auto bottom = bounds.getBottom();
        std::vector<float> levels; for (float db = maxdB; db >= mindB; db -= 6.0f) levels.push_back(db); levels.push_back(0.0f);
        std::sort(levels.begin(), levels.end()); levels.erase(std::unique(levels.begin(), levels.end(), [](float a, float b) { return approximatelyEqual(a, b); }), levels.end());
        auto labelFont = Font(FontOptions(9.f)); g.setFont(labelFont);
        const auto zeroLineCol = ColorScheme::getZeroDbLineBaseColor();
        const auto gridLineCol = ColorScheme::getAnalyzerGridBaseColor();
        const auto gridTextCol = ColorScheme::getScaleTextColor();
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
        // ... (drawSpectrumAndPeaks без изменений) ...
        using namespace juce;
        auto width = bounds.getWidth(); auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto numBins = displayData.size();
        auto sampleRate = processor.getSampleRate();
        if (numBins == 0 || peakHoldLevels.size() != numBins || sampleRate <= 0 || width <= 0) return;
        std::vector<Point<float>> spectrumPoints;
        std::vector<Point<float>> peakPointsVec;
        spectrumPoints.reserve(fftPointsSize > 0 ? fftPointsSize + 2 : numBins); // Оптимизация размера
        peakPointsVec.reserve(fftPointsSize > 0 ? fftPointsSize + 2 : numBins);

        const int firstBinToDraw = 1; // Начинаем с первого бина (индекс 0 это DC, часто его пропускают)         
        const float lowFreqRollOffEndBin = 5.0f;

        spectrumPoints.push_back({ left, bottom });
        peakPointsVec.push_back({ left, bottom });
        const auto spectrumFillBaseCol = ColorScheme::getSpectrumFillBaseColor();
        const auto spectrumLineCol = ColorScheme::getSpectrumLineColor();
        const auto peakHoldLineBaseCol = ColorScheme::getPeakHoldLineBaseColor();
        const auto overZeroLineCol = ColorScheme::getOverZeroDbLineColor();

        // Используем fftPoints для отрисовки, если они рассчитаны
        if (fftPointsSize > 0) {
            for (int i = 0; i < fftPointsSize; ++i) {
                const auto& pointInfo = fftPoints[i];
                float maxMagnitudeForPoint = mindB; // Начинаем с минимума для этого X
                float maxPeakForPoint = mindB;

                // Усредняем или берем максимум из displayData/peakHoldLevels для бинов, соответствующих этому X
                for (int bin = pointInfo.firstBinIndex; bin <= pointInfo.lastBinIndex; ++bin) {
                    if (bin >= 0 && bin < static_cast<int>(numBins)) { // Защита
                        maxMagnitudeForPoint = std::max(maxMagnitudeForPoint, displayData[bin]);
                        maxPeakForPoint = std::max(maxPeakForPoint, peakHoldLevels[bin]);
                    }
                }

                float x = left + pointInfo.x; // Используем сохраненную X-координату
                x = jlimit(left, right, x);

                float yDisplay = jlimit(top, jmap(maxMagnitudeForPoint, mindB, maxdB, bottom, top), bottom);
                float yPeak = jlimit(top, jmap(maxPeakForPoint, mindB, maxdB, bottom, top), bottom);

                spectrumPoints.push_back({ x, yDisplay });
                peakPointsVec.push_back({ x, yPeak });
            }
        }
        else { // Запасной вариант, если fftPoints не рассчитаны (рисуем все бины)
            for (size_t i = firstBinToDraw; i < numBins; ++i) { /* ... старая логика ... */ }
        }


        spectrumPoints.push_back({ right, bottom });
        peakPointsVec.push_back({ right, bottom });
        if (spectrumPoints.size() < 2) return;
        Path spectrumPath;
        spectrumPath.startNewSubPath(spectrumPoints[0]);
        for (size_t i = 1; i < spectrumPoints.size(); ++i) { /* ... старая логика сглаживания ... */
            const auto& p0 = spectrumPoints[i - 1]; const auto& p1 = spectrumPoints[i];
            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }; Point<float> cp2{ (p0.x + p1.x) * 0.5f, p1.y };
            spectrumPath.cubicTo(cp1, cp2, p1);
        }
        g.setColour(spectrumFillBaseCol.withAlpha(0.2f)); g.fillPath(spectrumPath);
        g.setColour(spectrumLineCol); g.strokePath(spectrumPath, PathStrokeType(1.5f));
        if (peakPointsVec.size() >= 2) {
            Path peakPath; peakPath.startNewSubPath(peakPointsVec[0]);
            for (size_t i = 1; i < peakPointsVec.size(); ++i) peakPath.lineTo(peakPointsVec[i]);
            g.setColour(peakHoldLineBaseCol.withAlpha(0.7f));
            g.strokePath(peakPath, PathStrokeType(1.0f));
        }
        // ... (логика отрисовки overZeroPath без изменений) ...
        Path overZeroPath; bool isCurrentlyAboveZero = false;
        float zeroDbY = jlimit(top, jmap(0.0f, mindB, maxdB, bottom, top), bottom);
        std::vector<Point<float>> currentSegmentPoints_oz; // Переименовал, чтобы не конфликтовать
        for (size_t i = 1; i < spectrumPoints.size() - 1; ++i)
        {
            const auto& currentPoint = spectrumPoints[i]; const float y = currentPoint.y;
            if (y <= zeroDbY)
            {
                if (!isCurrentlyAboveZero)
                {
                    isCurrentlyAboveZero = true; currentSegmentPoints_oz.clear();
                    const auto& prevPoint = spectrumPoints[i - 1];
                    if (prevPoint.y > zeroDbY) {
                        float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                        intersectX = jlimit(left, intersectX, right);
                        currentSegmentPoints_oz.push_back({ intersectX, zeroDbY });
                    }
                    else { currentSegmentPoints_oz.push_back(prevPoint); }
                    currentSegmentPoints_oz.push_back(currentPoint);
                }
                else { currentSegmentPoints_oz.push_back(currentPoint); }
            }
            else
            {
                if (isCurrentlyAboveZero)
                {
                    isCurrentlyAboveZero = false; const auto& prevPoint = spectrumPoints[i - 1];
                    float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                    intersectX = jlimit(left, intersectX, right);
                    currentSegmentPoints_oz.push_back({ intersectX, zeroDbY });
                    if (currentSegmentPoints_oz.size() >= 2) {
                        overZeroPath.startNewSubPath(currentSegmentPoints_oz[0]);
                        for (size_t p_idx = 1; p_idx < currentSegmentPoints_oz.size(); ++p_idx) {
                            const auto& p0 = currentSegmentPoints_oz[p_idx - 1]; const auto& p1 = currentSegmentPoints_oz[p_idx];
                            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                            overZeroPath.cubicTo(cp1, cp2, p1);
                        }
                    }
                }
            }
        }
        if (isCurrentlyAboveZero && currentSegmentPoints_oz.size() >= 2)
        {
            overZeroPath.startNewSubPath(currentSegmentPoints_oz[0]);
            for (size_t p_idx = 1; p_idx < currentSegmentPoints_oz.size(); ++p_idx) {
                const auto& p0 = currentSegmentPoints_oz[p_idx - 1]; const auto& p1 = currentSegmentPoints_oz[p_idx];
                Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                overZeroPath.cubicTo(cp1, cp2, p1);
            }
        }
        if (!overZeroPath.isEmpty()) {
            g.setColour(overZeroLineCol); g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
        }
    }

    float SpectrumAnalyzer::frequencyToX(float freq, float width) const
    {
        freq = juce::jlimit(minFreq, maxFreq, freq);
        // Проверка на допустимые значения для логарифма
        if (minFreq <= 0 || maxFreq <= 0 || minFreq >= maxFreq || freq <= 0 || width <= 0) return 0.0f;
        return width * (std::log(freq / minFreq) / std::log(maxFreq / minFreq));
    }
}
