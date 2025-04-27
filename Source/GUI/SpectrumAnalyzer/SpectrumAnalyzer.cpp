#include "SpectrumAnalyzer.h" // Âêëþ÷àåò PluginProcessor.h ÷åðåç ñåáÿ
#include <vector>
#include <cmath>      // Äëÿ std::log, std::exp, std::abs, std::max
#include <limits>     // Íå èñïîëüçóåòñÿ íàïðÿìóþ, íî ïîëåçíî äëÿ ÷èñëîâûõ ïðåäåëîâ
#include <algorithm>  // Äëÿ std::min, std::max, std::sort, std::unique
#include <juce_gui_basics/juce_gui_basics.h> // Äëÿ TextLayout, Graphics è ò.ä.
#include <juce_dsp/juce_dsp.h>
#include "../Source/PluginProcessor.h"// Äëÿ Decibels

// Óáåäèìñÿ, ÷òî ïðîñòðàíñòâî èìåí èç LookAndFeel äîñòóïíî äëÿ öâåòîâ (åñëè íóæíî)
// #include "../LookAndFeel.h" // Ðàñêîììåíòèðóéòå, åñëè ColorScheme îïðåäåëåí òàì

// --- Ïðîñòðàíñòâî èìåí ---
namespace MBRP_GUI
{

    // --- Õåëïåð äëÿ òåêñòà ---
    // Îïðåäåëåíèå ñòàòè÷åñêîé ôóíêöèè-÷ëåíà
    /*static*/float SpectrumAnalyzer::getTextLayoutWidth(const juce::String& text, const juce::Font& font)
    {
        juce::TextLayout textLayout;
        juce::AttributedString attrString(text);
        attrString.setFont(font);
        textLayout.createLayout(attrString, 10000.0f); // Use a large width to get the natural width
        return textLayout.getWidth();
    }

    //==============================================================================
    // Êîíñòðóêòîð
    SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& p) :
        processor{ p },
        // Инициализируем размером fftSize/2
        displayData(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        peakHoldLevels(size_t(MBRPAudioProcessor::fftSize / 2), mindB)
    {
        peakDbLevel.store(mindB);
        startTimerHz(60); // Таймер GUI
    }

    //==============================================================================
    // Òàéìåð - îñíîâíîé öèêë îáíîâëåíèÿ àíàëèçàòîðà
    void SpectrumAnalyzer::timerCallback()
    {
        bool newDataAvailable = false;
        // Проверяем флаг из процессора
        bool expected = true;
        if (processor.getIsFftDataReady().compare_exchange_strong(expected, false)) // Используем isFftDataReady
        {
            newDataAvailable = true;
            auto& magnitudes = processor.getFftMagnitudes(); // Получаем магнитуды
            auto numBins = magnitudes.size(); // fftSize / 2

            if (displayData.size() != numBins) displayData.resize(numBins);
            if (peakHoldLevels.size() != numBins) peakHoldLevels.resize(numBins);

            // КОЭФФИЦИЕНТ УСИЛЕНИЯ И НОРМАЛИЗАЦИЯ (Настройте!)
            const float gainAdjustment = 30.0f; // <-- НАСТРОЙТЕ УРОВЕНЬ
            const float windowGainCorrection = 2.0f; // Компенсация окна Hann (примерно)
            const float normalizationFactor = (gainAdjustment * 2.0f / static_cast<float>(MBRPAudioProcessor::fftSize)) * windowGainCorrection;

            float currentFramePeak = mindB;
            std::vector<float> latestDbData(numBins); // Временный вектор

            // 1. Конвертируем магнитуды в dB
            for (size_t i = 0; i < numBins; ++i) {
                float normalizedMagnitude = magnitudes[i] * normalizationFactor;
                latestDbData[i] = juce::Decibels::gainToDecibels(normalizedMagnitude, mindB);
                if (latestDbData[i] > currentFramePeak) currentFramePeak = latestDbData[i];
            }
            peakDbLevel.store(currentFramePeak);

            // 2. Обновляем displayData (EMA) и peakHoldLevels (Decay)
            for (size_t i = 0; i < numBins; ++i) {
                float newValDb = latestDbData[i]; float oldDisplayDb = displayData[i]; float oldPeakDb = peakHoldLevels[i];
                displayData[i] = smoothingAlpha * newValDb + (1.0f - smoothingAlpha) * oldDisplayDb;
                displayData[i] = std::max(mindB, displayData[i]);
                if (newValDb > oldPeakDb) peakHoldLevels[i] = newValDb;
                else peakHoldLevels[i] = std::max(displayData[i], juce::Decibels::gainToDecibels(juce::Decibels::decibelsToGain(oldPeakDb) * peakHoldDecayFactor, mindB));
            }
        }

        // Обновляем пики (затухание), если не было новых данных
        bool peakNeedsRepaint = false;
        if (!newDataAvailable) {
            for (size_t i = 0; i < peakHoldLevels.size(); ++i) {
                float oldPeakDb = peakHoldLevels[i];
                if (oldPeakDb > displayData[i] && oldPeakDb > mindB + 0.01f) {
                    peakHoldLevels[i] = std::max(displayData[i], juce::Decibels::gainToDecibels(juce::Decibels::decibelsToGain(oldPeakDb) * peakHoldDecayFactor, mindB));
                    if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) peakNeedsRepaint = true;
                }
                else if (oldPeakDb > mindB + 0.01f && peakHoldLevels[i] < displayData[i]) {
                    peakHoldLevels[i] = displayData[i];
                    if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) peakNeedsRepaint = true;
                }
            }
        }

        // Перерисовываем, если были новые данные или пики изменились
        if (newDataAvailable || peakNeedsRepaint) {
            repaint();
        }
    }

    // --- Îáðàáîòêà ñëåäóþùåãî êàäðà äàííûõ èç FIFO ---
    // Àäàïòèðîâàíî èç witte::SpectrumAnalyzer::drawNextFrame

    //==============================================================================
    // Ïåðåñ÷åò òî÷åê FFT <-> Ýêðàí (àäàïòèðîâàíî èç witte)

    //==============================================================================
    // Ñòàòè÷åñêàÿ ôóíêöèÿ ïîëó÷åíèÿ óðîâíÿ äëÿ òî÷êè ýêðàíà èç óñðåäíåííûõ ìàãíèòóä

    //==============================================================================
    // Îñíîâíàÿ ôóíêöèÿ îòðèñîâêè
    void SpectrumAnalyzer::paint(juce::Graphics& g)
    {
        using namespace juce;
        g.fillAll(backgroundColour);
        auto bounds = getLocalBounds().toFloat();
        auto graphBounds = bounds.reduced(1.f);

        drawFrequencyGrid(g, graphBounds);
        drawGainScale(g, graphBounds);
        drawSpectrumAndPeaks(g, graphBounds);

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
        // Óñòàíàâëèâàåì çàäåðæêó ïåðåñ÷åòà òî÷åê, ñàì ïåðåñ÷åò áóäåò â timerCallback
        static constexpr int framesToWaitBeforePaintingAfterResizing = 5;
        DBG("Resized called. Debounce set.");
    }

    //==============================================================================
    // --- Ôóíêöèè îòðèñîâêè ñåòîê è øêàë ---
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

    // --- Ôóíêöèÿ îòðèñîâêè ñïåêòðà è ïèêîâ ---
    void SpectrumAnalyzer::drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto width = bounds.getWidth(); auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto numBins = displayData.size(); // Èñïîëüçóåì displayData
        auto sampleRate = processor.getSampleRate();

        // Ïðîâåðêà íà âàëèäíîñòü äàííûõ
        if (numBins == 0 || peakHoldLevels.size() != numBins || sampleRate <= 0 || width <= 0) return;

        // Âåêòîðû äëÿ õðàíåíèÿ òî÷åê êðèâûõ
        std::vector<Point<float>> spectrumPoints;
        std::vector<Point<float>> peakPointsVec;
        spectrumPoints.reserve(numBins); // Ïðåäâàðèòåëüíîå âûäåëåíèå ïàìÿòè
        peakPointsVec.reserve(numBins);

        // Êîíñòàíòû äëÿ îáðàáîòêè Í×
        const int firstBinToDraw = 2;           // Íà÷èíàåì ðèñîâàòü ñî 2-ãî áèíà (ïðîïóñêàåì DC è 1-é)
        const float lowFreqRollOffEndBin = 5.0f;  //  (ðàññ÷èòûâàëîñü äëÿ 10.f)Äî êàêîãî áèíà ïðèìåíÿòü îñëàáëåíèå (ëîó)

        // Äîáàâëÿåì íà÷àëüíûå òî÷êè âíèçó ñëåâà
        spectrumPoints.push_back({ left, bottom });
        peakPointsVec.push_back({ left, bottom });

        // Ñîáèðàåì òî÷êè äëÿ îñíîâíîé (EMA) è ïèêîâîé (Peak Hold) ëèíèé
        for (size_t i = firstBinToDraw; i < numBins; ++i)
        {
            float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);

            // Îáðàáàòûâàåì òîëüêî ÷àñòîòû â âèäèìîì äèàïàçîíå
            if (freq >= minFreq && freq <= maxFreq)
            {
                float x = left + frequencyToX(freq, width);
                x = jlimit(left, right, x); // Îãðàíè÷èâàåì X ãðàíèöàìè

                float displayDb = displayData[i]; // Ñãëàæåííîå çíà÷åíèå äëÿ îñíîâíîé ëèíèè
                float peakDb = peakHoldLevels[i]; // Ïèêîâîå çíà÷åíèå

                // --- Âèçóàëüíîå îñëàáëåíèå ñàìûõ íèçêèõ ÷àñòîò ---
                // Ïðèìåíÿåì ê îáîèì çíà÷åíèÿì äëÿ êîíñèñòåíòíîñòè îòðèñîâêè
                float lowFreqAttenuation = 1.0f;
                if (i < firstBinToDraw + lowFreqRollOffEndBin) {
                    lowFreqAttenuation = juce::jmap(float(i), float(firstBinToDraw - 1), float(firstBinToDraw + lowFreqRollOffEndBin), 0.45f, 1.0f); // Îò 0.3 äî 1.0 (ëîó áûëî 0.3f)
                    // Îñëàáëÿåì çíà÷åíèå dB îòíîñèòåëüíî mindB
                    displayDb = mindB + (displayDb - mindB) * lowFreqAttenuation;
                    peakDb = mindB + (peakDb - mindB) * lowFreqAttenuation;
                }
                // --------------------------------------------------

                // Ìàïïèíã â Y-êîîðäèíàòû è îãðàíè÷åíèå ãðàíèöàìè
                float yDisplay = jlimit(top, jmap(displayDb, mindB, maxdB, bottom, top), bottom);
                float yPeak = jlimit(top, jmap(peakDb, mindB, maxdB, bottom, top), bottom);

                // Äîáàâëÿåì òî÷êó äëÿ îñíîâíîé êðèâîé
                spectrumPoints.push_back({ x, yDisplay });

                // Äîáàâëÿåì òî÷êó äëÿ ïèêîâîé êðèâîé (ñ îïòèìèçàöèåé äëÿ âåðòèêàëüíûõ ëèíèé)
                if (peakPointsVec.empty() || !juce::approximatelyEqual(x, peakPointsVec.back().x)) {
                    peakPointsVec.push_back({ x, yPeak }); // Íîâàÿ X êîîðäèíàòà
                }
                else {
                    // Òà æå X êîîðäèíàòà - áåðåì ñàìûé âûñîêèé ïèê (ìèíèìàëüíûé Y)
                    peakPointsVec.back().y = std::min(peakPointsVec.back().y, yPeak);
                }
            }
            // Ïðåðûâàåì öèêë, åñëè âûøëè äàëåêî çà ïðàâóþ ãðàíèöó
            if (freq > maxFreq * 1.05) break;
        }
        // Äîáàâëÿåì êîíå÷íûå òî÷êè âíèçó ñïðàâà
        spectrumPoints.push_back({ right, bottom });
        peakPointsVec.push_back({ right, bottom });

        // Ïðîâåðêà, äîñòàòî÷íî ëè òî÷åê äëÿ ðèñîâàíèÿ
        if (spectrumPoints.size() < 2) return;

        // --- Ðèñóåì îñíîâíóþ êðèâóþ (EMA) ñ cubicTo ---
        Path spectrumPath;
        spectrumPath.startNewSubPath(spectrumPoints[0]);
        for (size_t i = 1; i < spectrumPoints.size(); ++i) {
            const auto& p0 = spectrumPoints[i - 1];
            const auto& p1 = spectrumPoints[i];
            // Êîíòðîëüíûå òî÷êè äëÿ êóáè÷åñêîé êðèâîé Áåçüå
            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y };
            Point<float> cp2{ (p0.x + p1.x) * 0.5f, p1.y };
            spectrumPath.cubicTo(cp1, cp2, p1); // Ïëàâíàÿ ëèíèÿ
        }
        // Çàëèâêà ïîä îñíîâíîé êðèâîé
        g.setColour(spectrumFillColour);
        g.fillPath(spectrumPath);
        // Îáâîäêà îñíîâíîé êðèâîé
        g.setColour(spectrumLineColour);
        g.strokePath(spectrumPath, PathStrokeType(1.5f)); // Ãîëóáàÿ ëèíèÿ

        // --- Ðèñóåì ïèêîâóþ êðèâóþ ñ lineTo ---
        if (peakPointsVec.size() >= 2) {
            Path peakPath;
            peakPath.startNewSubPath(peakPointsVec[0]);
            for (size_t i = 1; i < peakPointsVec.size(); ++i) {
                peakPath.lineTo(peakPointsVec[i]); // Ïðÿìàÿ ëèíèÿ äëÿ ïèêîâ
            }
            g.setColour(peakHoldLineColour);
            g.strokePath(peakPath, PathStrokeType(1.0f)); // Æåëòîâàòàÿ òîíêàÿ ëèíèÿ
        }

        // --- ÎÏÒÈÌÈÇÈÐÎÂÀÍÍÀß ÎÒÐÈÑÎÂÊÀ ÊÐÀÑÍÎÉ ËÈÍÈÈ (> 0 äÁ) ---
        Path overZeroPath;
        bool isCurrentlyAboveZero = false;
        float zeroDbY = jlimit(top, jmap(0.0f, mindB, maxdB, bottom, top), bottom); // Y íóëÿ

        std::vector<Point<float>> currentSegmentPoints;

        // Èòåðèðóåì ïî òî÷êàì îñíîâíîé êðèâîé (spectrumPoints), òàê êàê îíè óæå ñîäåðæàò îñëàáëåííûå Y
        for (size_t i = 1; i < spectrumPoints.size() - 1; ++i) // Ïðîïóñêàåì ïåðâóþ è ïîñëåäíþþ òî÷êó (îíè âñåãäà íà bottom)
        {
            const auto& currentPoint = spectrumPoints[i];
            const float y = currentPoint.y;
            const float x = currentPoint.x;

            // --- ÏÐÎÂÅÐÊÀ ÏÎ Y êîîðäèíàòå ---
            // Ìåíüøåå çíà÷åíèå Y îçíà÷àåò áîëåå âûñîêèé óðîâåíü íà ãðàôèêå
            if (y <= zeroDbY) // Åñëè òî÷êà ÂÛØÅ èëè ÍÀ ëèíèè íóëÿ
            {
                if (!isCurrentlyAboveZero) // Íà÷àëî íîâîãî ñåãìåíòà > 0
                {
                    isCurrentlyAboveZero = true;
                    currentSegmentPoints.clear();
                    const auto& prevPoint = spectrumPoints[i - 1]; // Áåðåì ïðåäûäóùóþ òî÷êó

                    // Åñëè ïðåäûäóùàÿ òî÷êà áûëà ÍÈÆÅ íóëÿ (y > zeroDbY)
                    if (prevPoint.y > zeroDbY) {
                        // Íàõîäèì òî÷êó ïåðåñå÷åíèÿ ñ zeroDbY ïî X (ïðîñòàÿ ëèíåéíàÿ èíòåðïîëÿöèÿ)
                        float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                        intersectX = jlimit(left, intersectX, right); // Îãðàíè÷èâàåì
                        currentSegmentPoints.push_back({ intersectX, zeroDbY });
                    }
                    else {
                        // Åñëè ïðåäûäóùàÿ òî÷êà áûëà òîæå >= 0 (íà ëèíèè), íà÷èíàåì ñ íåå
                        currentSegmentPoints.push_back(prevPoint);
                    }
                    currentSegmentPoints.push_back(currentPoint); // Äîáàâëÿåì òåêóùóþ òî÷êó
                }
                else // Ïðîäîëæåíèå ñåãìåíòà > 0
                {
                    currentSegmentPoints.push_back(currentPoint);
                }
            }
            else // Òåêóùàÿ òî÷êà ÍÈÆÅ íóëÿ (y > zeroDbY)
            {
                if (isCurrentlyAboveZero) // Êîíåö ñåãìåíòà > 0
                {
                    isCurrentlyAboveZero = false;
                    const auto& prevPoint = spectrumPoints[i - 1]; // Ïðåäûäóùàÿ òî÷êà (áûëà <= zeroDbY)

                    // Íàõîäèì òî÷êó ïåðåñå÷åíèÿ ñ zeroDbY
                    float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                    intersectX = jlimit(left, intersectX, right);
                    currentSegmentPoints.push_back({ intersectX, zeroDbY }); // Çàêàí÷èâàåì íà ëèíèè íóëÿ

                    // Äîáàâëÿåì ñåãìåíò â îáùèé ïóòü
                    if (currentSegmentPoints.size() >= 2) {
                        overZeroPath.startNewSubPath(currentSegmentPoints[0]);
                        for (size_t p_idx = 1; p_idx < currentSegmentPoints.size(); ++p_idx) {
                            const auto& p0 = currentSegmentPoints[p_idx - 1]; const auto& p1 = currentSegmentPoints[p_idx];
                            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                            overZeroPath.cubicTo(cp1, cp2, p1);
                        }
                    }
                }
                // Åñëè è òàê áûëè íèæå íóëÿ, íè÷åãî íå äåëàåì
            }
        }

        // Îáðàáîòêà ïîñëåäíåãî ñåãìåíòà (åñëè çàêîí÷èëñÿ âûøå íóëÿ)
        if (isCurrentlyAboveZero && currentSegmentPoints.size() >= 2)
        {
            // Äîáàâëÿåì ïîñëåäíþþ òî÷êó íà ïðàâîì êðàþ íà óðîâíå 0 äÁ? Èëè íà óðîâíå ïîñëåäíåé òî÷êè?
            // Ëó÷øå çàêîí÷èòü íà ïîñëåäíåé ðåàëüíîé òî÷êå, à íå èñêóññòâåííî îïóñêàòü äî íóëÿ.
            overZeroPath.startNewSubPath(currentSegmentPoints[0]);
            for (size_t p_idx = 1; p_idx < currentSegmentPoints.size(); ++p_idx) {
                const auto& p0 = currentSegmentPoints[p_idx - 1]; const auto& p1 = currentSegmentPoints[p_idx];
                Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                overZeroPath.cubicTo(cp1, cp2, p1);
            }
        }

        // Ðèñóåì ÂÅÑÜ êðàñíûé ïóòü ÎÄÍÈÌ âûçîâîì
        if (!overZeroPath.isEmpty()) {
            g.setColour(overZeroDbLineColour);
            g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
        }
        // --- ÊÎÍÅÖ ÁËÎÊÀ ÊÐÀÑÍÎÉ ËÈÍÈÈ ---
    }
    // --- Îòðèñîâêà ìàðêåðîâ êðîññîâåðà ---
    // --- Ïðåîáðàçîâàíèå ÷àñòîòû â X ---
    float SpectrumAnalyzer::frequencyToX(float freq, float width) const
    {
        freq = juce::jlimit(minFreq, maxFreq, freq);
        if (maxFreq / minFreq <= 1.0f || freq <= 0 || width <= 0) return 0.0f;
        return width * (std::log(freq / minFreq) / std::log(maxFreq / minFreq));
    }

} // namespace MBRP_GUI