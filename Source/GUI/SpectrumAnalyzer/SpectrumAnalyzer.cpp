#include "SpectrumAnalyzer.h" // Включает PluginProcessor.h через себя
#include <vector>
#include <cmath>      // Для std::log, std::exp, std::abs, std::max
#include <limits>     // Не используется напрямую, но полезно для числовых пределов
#include <algorithm>  // Для std::min, std::max, std::sort, std::unique
#include <juce_gui_basics/juce_gui_basics.h> // Для TextLayout, Graphics и т.д.
#include <juce_dsp/juce_dsp.h>             // Для Decibels

// Убедимся, что пространство имен из LookAndFeel доступно для цветов (если нужно)
// #include "../LookAndFeel.h" // Раскомментируйте, если ColorScheme определен там

// --- Пространство имен ---
namespace MBRP_GUI
{

    // --- Хелпер для текста ---
    // Определение статической функции-члена
    /*static*/float SpectrumAnalyzer::getTextLayoutWidth(const juce::String& text, const juce::Font& font)
    {
        juce::TextLayout textLayout;
        juce::AttributedString attrString(text);
        attrString.setFont(font);
        textLayout.createLayout(attrString, 10000.0f); // Use a large width to get the natural width
        return textLayout.getWidth();
    }

    //==============================================================================
    // Конструктор
    SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& p) :
        processor{ p },
        // Инициализируем векторы с размером fftSize/2 и значением mindB
        displayData(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        peakHoldLevels(size_t(MBRPAudioProcessor::fftSize / 2), mindB),
        fftPoints(MBRPAudioProcessor::fftSize) // Выделяем память для fftPoints (структура содержит int)
    {
        peakDbLevel.store(mindB);   // Инициализируем атомарный пик
        avgInput.clear();           // Очищаем буферы усреднения
        avgOutput.clear();
        startTimerHz(60);           // Устанавливаем частоту таймера (например, 60 Гц)
    }

    //==============================================================================
    // Таймер - основной цикл обновления анализатора
    void SpectrumAnalyzer::timerCallback()
    {
        bool newDataAvailable = processor.nextFFTBlockReady.load(); // Проверяем флаг из процессора

        if (newDataAvailable)
        {
            drawNextFrame(); // Обрабатываем новые данные FFT из FIFO
            processor.nextFFTBlockReady.store(false); // Сбрасываем флаг
        }

        // Обработка задержки после изменения размера
        if (resizeDebounceInFrames > 0)
        {
            --resizeDebounceInFrames;
            if (resizeDebounceInFrames == 0) // Время пересчитать точки
            {
                recalculateFftPoints();
                // Запросим перерисовку после пересчета, даже если не было новых данных
                if (!newDataAvailable) repaint(); // Перерисовать, если новых данных не было, но ресайз закончился
            }
            // Пока идет ресайз/задержка, не обновляем пики и не перерисовываем активно
            return;
        }

        // Обновляем пики (применяем затухание), даже если не было новых данных FFT
        bool peakNeedsRepaint = false;
        for (size_t i = 0; i < peakHoldLevels.size(); ++i)
        {
            float oldPeakDb = peakHoldLevels[i];
            // Затухаем пик, если он выше основной линии и выше порога mindB
            if (oldPeakDb > displayData[i] && oldPeakDb > mindB + 0.01f)
            {
                peakHoldLevels[i] = std::max(displayData[i], juce::Decibels::gainToDecibels(juce::Decibels::decibelsToGain(oldPeakDb) * peakHoldDecayFactor, mindB));
                if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) {
                    peakNeedsRepaint = true; // Отмечаем, что пик изменился
                }
            }
            else if (oldPeakDb > mindB + 0.01f && peakHoldLevels[i] < displayData[i]) {
                // Если пик упал ниже основной линии, устанавливаем его равным основной линии
                peakHoldLevels[i] = displayData[i];
                if (!juce::approximatelyEqual(peakHoldLevels[i], oldPeakDb)) {
                    peakNeedsRepaint = true;
                }
            }
        }

        // Перерисовываем, если были новые данные ИЛИ если изменились пики
        if (newDataAvailable || peakNeedsRepaint) {
            repaint();
        }
    }


    // --- Обработка следующего кадра данных из FIFO ---
    // Адаптировано из witte::SpectrumAnalyzer::drawNextFrame
    void SpectrumAnalyzer::drawNextFrame()
    {
        bool newDataProcessed = false; // Флаг для индикации обработки новых данных FFT

        // --- Обработка входного FIFO ---
        while (processor.abstractFifoInput.getNumReady() >= MBRPAudioProcessor::fftSize) // Обрабатываем все доступные блоки
        {
            fftBufferInput.clear(); // Очищаем временный буфер для FFT
            int start1, block1, start2, block2;
            // Готовим FIFO к чтению
            processor.abstractFifoInput.prepareToRead(MBRPAudioProcessor::fftSize, start1, block1, start2, block2);

            // --- Копируем данные из FIFO процессора во временный буфер анализатора ---
            const int audioFifoSize = processor.audioFifoInput.getNumSamples();
            if (audioFifoSize > 0) { // Доп. проверка на размер буфера FIFO
                if (block1 > 0) fftBufferInput.copyFrom(0, 0, processor.audioFifoInput.getReadPointer(0, start1 % audioFifoSize), block1);
                if (block2 > 0) fftBufferInput.copyFrom(0, block1, processor.audioFifoInput.getReadPointer(0, start2 % audioFifoSize), block2);
            }
            // --- ---

            // Сообщаем FIFO процессора, что мы прочитали данные
            processor.abstractFifoInput.finishedRead(block1 + block2);

            // Применяем окно Ханна
            hannWindow.multiplyWithWindowingTable(fftBufferInput.getWritePointer(0), static_cast<size_t>(MBRPAudioProcessor::fftSize));
            // Выполняем БПФ (только магнитуды)
            fftInput.performFrequencyOnlyForwardTransform(fftBufferInput.getWritePointer(0));

            // --- Усреднение результата FFT ---
            {
                juce::ScopedLock lockedForAvgUpdate(pathCreationLock);
                // Вычитаем самый старый кадр из суммы (канал 0)
                avgInput.addFrom(0, 0, avgInput.getReadPointer(avgInputPtr), avgInput.getNumSamples(), -1.0f);
                // Нормализация для усреднения (делим на кол-во кадров)
                float normFactor = 1.0f / (static_cast<float>(avgInput.getNumChannels() - 1));
                avgInput.copyFrom(avgInputPtr, 0, fftBufferInput.getReadPointer(0), avgInput.getNumSamples(), normFactor);
                // Добавляем новый нормализованный кадр к общей сумме (в канал 0)
                avgInput.addFrom(0, 0, avgInput.getReadPointer(avgInputPtr), avgInput.getNumSamples());
            }
            // Передвигаем указатель на следующий слот усреднения
            if (++avgInputPtr >= avgInput.getNumChannels()) avgInputPtr = 1;

            newDataProcessed = true; // Отмечаем, что обработали новые данные
        }

        // --- Обработка выходного FIFO (Если используется) ---
        // while (processor.abstractFifoOutput.getNumReady() >= MBRPAudioProcessor::fftSize) { /* ... */ }


        // --- Обновление отображаемых данных, если были новые FFT ---
        if (newDataProcessed)
        {
            auto numBins = avgInput.getNumSamples(); // fftSize / 2
            std::vector<float> latestDbData(numBins); // Вектор для *текущих* dB

            if (displayData.size() != numBins) displayData.resize(numBins);
            if (peakHoldLevels.size() != numBins) peakHoldLevels.resize(numBins);

            // Получаем указатель на усредненные магнитуды
            const float* averagedMagnitudes = nullptr;
            {
                juce::ScopedLock lockedForAvgRead(pathCreationLock); // Блокировка на чтение
                if (avgInput.getNumSamples() > 0) averagedMagnitudes = avgInput.getReadPointer(0);
            }
            if (averagedMagnitudes == nullptr) return; // Не удалось получить данные

            // --- КОЭФФИЦИЕНТ УСИЛЕНИЯ (НАСТРОЙКА!) ---
            const float gainAdjustment = -55.0f; // <-- НАСТРОЙТЕ ЭТО!
            const float gainMultiplier = juce::Decibels::decibelsToGain(gainAdjustment);
            // -------------------------------------------

            float currentFramePeak = mindB;

            // 1. Вычисляем уровни дБ из УСРЕДНЕННЫХ магнитуд
            for (size_t i = 0; i < numBins; ++i)
            {
                float finalMagnitude = averagedMagnitudes[i] * gainMultiplier;
                latestDbData[i] = juce::Decibels::gainToDecibels(finalMagnitude, mindB);
                if (latestDbData[i] > currentFramePeak) currentFramePeak = latestDbData[i];
            }
            peakDbLevel.store(currentFramePeak); // Обновляем общий пик

            // 2. Обновляем displayData (EMA) и peakHoldLevels (Decay)
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
            // Перерисовка будет вызвана из timerCallback после этого метода
        }
    }

    //==============================================================================
    // Пересчет точек FFT <-> Экран (адаптировано из witte)
    void SpectrumAnalyzer::recalculateFftPoints()
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto width = bounds.getWidth();
        if (width <= 0) { fftPointsSize = 0; return; }

        const auto sampleRate = static_cast<float> (processor.getSampleRate());
        const auto fftSizeHalved = static_cast<int> (MBRPAudioProcessor::fftSize / 2);
        if (sampleRate <= 0 || fftSizeHalved <= 0) { fftPointsSize = 0; return; } // Доп. проверки

        const float minLogFreq = 20.0f;
        const float maxLogFreq = sampleRate / 2.0f;
        if (maxLogFreq <= minLogFreq) { fftPointsSize = 0; return; }
        const float logRange = std::log(maxLogFreq / minLogFreq);

        if (fftPoints.size() != MBRPAudioProcessor::fftSize) // Убедимся, что размер вектора достаточен
            fftPoints.resize(MBRPAudioProcessor::fftSize);

        fftPointsSize = 0;
        int lastX = -1;

        for (int i = 1; i < fftSizeHalved; ++i) // Начинаем с бина 1
        {
            const float freq = sampleRate * static_cast<float>(i) / static_cast<float>(MBRPAudioProcessor::fftSize);
            if (freq < minLogFreq) continue;

            const float logPos = std::log(freq / minLogFreq) / logRange;
            const int x = juce::roundToInt(logPos * width);

            if (x >= width) break; // Вышли за пределы

            if (x > lastX) // Новая точка на экране
            {
                if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i - 1;
                if (fftPointsSize < (int)fftPoints.size()) {
                    fftPoints[fftPointsSize].firstBinIndex = i;
                    fftPoints[fftPointsSize].x = x;
                    fftPointsSize++;
                    lastX = x;
                }
                else { jassertfalse; break; } // Ошибка размера вектора
            }
            if (fftPointsSize > 0) fftPoints[fftPointsSize - 1].lastBinIndex = i; // Обновляем последний бин
        }
        if (fftPointsSize > 0) { // Ограничиваем последний индекс
            fftPoints[fftPointsSize - 1].lastBinIndex = std::min(fftPoints[fftPointsSize - 1].lastBinIndex, fftSizeHalved - 1);
        }
        DBG("Recalculated FFT points. Count: " << fftPointsSize << " for width: " << width);
    }

    //==============================================================================
    // Статическая функция получения уровня для точки экрана из усредненных магнитуд
    /*static*/ float SpectrumAnalyzer::getFftPointLevel(const float* averagedMagnitudes, const fftPoint& point)
    {
        float maxMagnitude = 0.0f;
        const int numSamples = MBRPAudioProcessor::fftSize / 2;

        for (int i = point.firstBinIndex; i <= point.lastBinIndex; ++i) {
            if (i >= 0 && i < numSamples) { // Проверка границ
                if (averagedMagnitudes[i] > maxMagnitude) maxMagnitude = averagedMagnitudes[i];
            }
        }
        // Усиление gainAdjustment применяется в drawNextFrame перед конвертацией в dB
        return juce::Decibels::gainToDecibels(maxMagnitude, mindB); // Конвертируем в dB здесь? Или передавать dB?
        // Если усиление в drawNextFrame, то здесь просто конвертация.
        // НО: В drawNextFrame уже конвертировали в latestDbData!
        // Значит, getFftPointLevel сейчас не нужна, если drawSpectrumAndPeaks
        // будет работать с displayData/peakHoldLevels напрямую.
        // ОСТАВИМ пока, но возможно, она не используется.
    }

    //==============================================================================
    // Основная функция отрисовки
    void SpectrumAnalyzer::paint(juce::Graphics& g)
    {
        using namespace juce;
        g.fillAll(backgroundColour);
        auto bounds = getLocalBounds().toFloat();
        auto graphBounds = bounds.reduced(1.f);

        // Пересчет точек, если окно было только что создано/изменено
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
        // Устанавливаем задержку пересчета точек, сам пересчет будет в timerCallback
        static constexpr int framesToWaitBeforePaintingAfterResizing = 5;
        resizeDebounceInFrames = framesToWaitBeforePaintingAfterResizing;
        DBG("Resized called. Debounce set.");
    }

    //==============================================================================
    // --- Функции отрисовки сеток и шкал ---
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

    // --- Функция отрисовки спектра и пиков ---
    void SpectrumAnalyzer::drawSpectrumAndPeaks(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        using namespace juce;
        auto width = bounds.getWidth(); auto top = bounds.getY(); auto bottom = bounds.getBottom();
        auto left = bounds.getX(); auto right = bounds.getRight();
        auto numBins = displayData.size(); // Используем displayData
        auto sampleRate = processor.getSampleRate();

        // Проверка на валидность данных
        if (numBins == 0 || peakHoldLevels.size() != numBins || sampleRate <= 0 || width <= 0) return;

        // Векторы для хранения точек кривых
        std::vector<Point<float>> spectrumPoints;
        std::vector<Point<float>> peakPointsVec;
        spectrumPoints.reserve(numBins); // Предварительное выделение памяти
        peakPointsVec.reserve(numBins);

        // Константы для обработки НЧ
        const int firstBinToDraw = 2;           // Начинаем рисовать со 2-го бина (пропускаем DC и 1-й)
        const float lowFreqRollOffEndBin = 5.0f;  //  (рассчитывалось для 10.f)До какого бина применять ослабление (лоу)

        // Добавляем начальные точки внизу слева
        spectrumPoints.push_back({ left, bottom });
        peakPointsVec.push_back({ left, bottom });

        // Собираем точки для основной (EMA) и пиковой (Peak Hold) линий
        for (size_t i = firstBinToDraw; i < numBins; ++i)
        {
            float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);

            // Обрабатываем только частоты в видимом диапазоне
            if (freq >= minFreq && freq <= maxFreq)
            {
                float x = left + frequencyToX(freq, width);
                x = jlimit(left, right, x); // Ограничиваем X границами

                float displayDb = displayData[i]; // Сглаженное значение для основной линии
                float peakDb = peakHoldLevels[i]; // Пиковое значение

                // --- Визуальное ослабление самых низких частот ---
                // Применяем к обоим значениям для консистентности отрисовки
                float lowFreqAttenuation = 1.0f;
                if (i < firstBinToDraw + lowFreqRollOffEndBin) {
                    lowFreqAttenuation = juce::jmap(float(i), float(firstBinToDraw - 1), float(firstBinToDraw + lowFreqRollOffEndBin), 0.6f, 1.0f); // От 0.3 до 1.0 (лоу было 0.3f)
                    // Ослабляем значение dB относительно mindB
                    displayDb = mindB + (displayDb - mindB) * lowFreqAttenuation;
                    peakDb = mindB + (peakDb - mindB) * lowFreqAttenuation;
                }
                // --------------------------------------------------

                // Маппинг в Y-координаты и ограничение границами
                float yDisplay = jlimit(top, jmap(displayDb, mindB, maxdB, bottom, top), bottom);
                float yPeak = jlimit(top, jmap(peakDb, mindB, maxdB, bottom, top), bottom);

                // Добавляем точку для основной кривой
                spectrumPoints.push_back({ x, yDisplay });

                // Добавляем точку для пиковой кривой (с оптимизацией для вертикальных линий)
                if (peakPointsVec.empty() || !juce::approximatelyEqual(x, peakPointsVec.back().x)) {
                    peakPointsVec.push_back({ x, yPeak }); // Новая X координата
                }
                else {
                    // Та же X координата - берем самый высокий пик (минимальный Y)
                    peakPointsVec.back().y = std::min(peakPointsVec.back().y, yPeak);
                }
            }
            // Прерываем цикл, если вышли далеко за правую границу
            if (freq > maxFreq * 1.05) break;
        }
        // Добавляем конечные точки внизу справа
        spectrumPoints.push_back({ right, bottom });
        peakPointsVec.push_back({ right, bottom });

        // Проверка, достаточно ли точек для рисования
        if (spectrumPoints.size() < 2) return;

        // --- Рисуем основную кривую (EMA) с cubicTo ---
        Path spectrumPath;
        spectrumPath.startNewSubPath(spectrumPoints[0]);
        for (size_t i = 1; i < spectrumPoints.size(); ++i) {
            const auto& p0 = spectrumPoints[i - 1];
            const auto& p1 = spectrumPoints[i];
            // Контрольные точки для кубической кривой Безье
            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y };
            Point<float> cp2{ (p0.x + p1.x) * 0.5f, p1.y };
            spectrumPath.cubicTo(cp1, cp2, p1); // Плавная линия
        }
        // Заливка под основной кривой
        g.setColour(spectrumFillColour);
        g.fillPath(spectrumPath);
        // Обводка основной кривой
        g.setColour(spectrumLineColour);
        g.strokePath(spectrumPath, PathStrokeType(1.5f)); // Голубая линия

        // --- Рисуем пиковую кривую с lineTo ---
        if (peakPointsVec.size() >= 2) {
            Path peakPath;
            peakPath.startNewSubPath(peakPointsVec[0]);
            for (size_t i = 1; i < peakPointsVec.size(); ++i) {
                peakPath.lineTo(peakPointsVec[i]); // Прямая линия для пиков
            }
            g.setColour(peakHoldLineColour);
            g.strokePath(peakPath, PathStrokeType(1.0f)); // Желтоватая тонкая линия
        }

        // --- ОПТИМИЗИРОВАННАЯ ОТРИСОВКА КРАСНОЙ ЛИНИИ (> 0 дБ) ---
        Path overZeroPath;
        bool isCurrentlyAboveZero = false;
        float zeroDbY = jlimit(top, jmap(0.0f, mindB, maxdB, bottom, top), bottom); // Y нуля

        std::vector<Point<float>> currentSegmentPoints;

        // Итерируем по точкам основной кривой (spectrumPoints), так как они уже содержат ослабленные Y
        for (size_t i = 1; i < spectrumPoints.size() - 1; ++i) // Пропускаем первую и последнюю точку (они всегда на bottom)
        {
            const auto& currentPoint = spectrumPoints[i];
            const float y = currentPoint.y;
            const float x = currentPoint.x;

            // --- ПРОВЕРКА ПО Y координате ---
            // Меньшее значение Y означает более высокий уровень на графике
            if (y <= zeroDbY) // Если точка ВЫШЕ или НА линии нуля
            {
                if (!isCurrentlyAboveZero) // Начало нового сегмента > 0
                {
                    isCurrentlyAboveZero = true;
                    currentSegmentPoints.clear();
                    const auto& prevPoint = spectrumPoints[i - 1]; // Берем предыдущую точку

                    // Если предыдущая точка была НИЖЕ нуля (y > zeroDbY)
                    if (prevPoint.y > zeroDbY) {
                        // Находим точку пересечения с zeroDbY по X (простая линейная интерполяция)
                        float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                        intersectX = jlimit(left, intersectX, right); // Ограничиваем
                        currentSegmentPoints.push_back({ intersectX, zeroDbY });
                    }
                    else {
                        // Если предыдущая точка была тоже >= 0 (на линии), начинаем с нее
                        currentSegmentPoints.push_back(prevPoint);
                    }
                    currentSegmentPoints.push_back(currentPoint); // Добавляем текущую точку
                }
                else // Продолжение сегмента > 0
                {
                    currentSegmentPoints.push_back(currentPoint);
                }
            }
            else // Текущая точка НИЖЕ нуля (y > zeroDbY)
            {
                if (isCurrentlyAboveZero) // Конец сегмента > 0
                {
                    isCurrentlyAboveZero = false;
                    const auto& prevPoint = spectrumPoints[i - 1]; // Предыдущая точка (была <= zeroDbY)

                    // Находим точку пересечения с zeroDbY
                    float intersectX = prevPoint.x + (currentPoint.x - prevPoint.x) * (zeroDbY - prevPoint.y) / (currentPoint.y - prevPoint.y);
                    intersectX = jlimit(left, intersectX, right);
                    currentSegmentPoints.push_back({ intersectX, zeroDbY }); // Заканчиваем на линии нуля

                    // Добавляем сегмент в общий путь
                    if (currentSegmentPoints.size() >= 2) {
                        overZeroPath.startNewSubPath(currentSegmentPoints[0]);
                        for (size_t p_idx = 1; p_idx < currentSegmentPoints.size(); ++p_idx) {
                            const auto& p0 = currentSegmentPoints[p_idx - 1]; const auto& p1 = currentSegmentPoints[p_idx];
                            Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                            overZeroPath.cubicTo(cp1, cp2, p1);
                        }
                    }
                }
                // Если и так были ниже нуля, ничего не делаем
            }
        }

        // Обработка последнего сегмента (если закончился выше нуля)
        if (isCurrentlyAboveZero && currentSegmentPoints.size() >= 2)
        {
            // Добавляем последнюю точку на правом краю на уровне 0 дБ? Или на уровне последней точки?
            // Лучше закончить на последней реальной точке, а не искусственно опускать до нуля.
            overZeroPath.startNewSubPath(currentSegmentPoints[0]);
            for (size_t p_idx = 1; p_idx < currentSegmentPoints.size(); ++p_idx) {
                const auto& p0 = currentSegmentPoints[p_idx - 1]; const auto& p1 = currentSegmentPoints[p_idx];
                Point<float> cp1{ (p0.x + p1.x) * 0.5f, p0.y }, cp2{ (p0.x + p1.x) * 0.5f, p1.y };
                overZeroPath.cubicTo(cp1, cp2, p1);
            }
        }

        // Рисуем ВЕСЬ красный путь ОДНИМ вызовом
        if (!overZeroPath.isEmpty()) {
            g.setColour(overZeroDbLineColour);
            g.strokePath(overZeroPath, juce::PathStrokeType(1.5f));
        }
        // --- КОНЕЦ БЛОКА КРАСНОЙ ЛИНИИ ---
    }
    // --- Отрисовка маркеров кроссовера ---
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

    // --- Преобразование частоты в X ---
    float SpectrumAnalyzer::frequencyToX(float freq, float width) const
    {
        freq = juce::jlimit(minFreq, maxFreq, freq);
        if (maxFreq / minFreq <= 1.0f || freq <= 0 || width <= 0) return 0.0f;
        return width * (std::log(freq / minFreq) / std::log(maxFreq / minFreq));
    }

} // namespace MBRP_GUI