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
    displayData(size_t(MBRPAudioProcessor::fftSize / 2))
{
    peakDbLevel.store(minDb); // Initialize atomic peak level
    // Fill initial spectrum data with minimum dB value
    std::fill(displayData.begin(), displayData.end(), minDb);
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
    bool expected = true;
    if (audioProcessor.getIsFftDataReady().compare_exchange_strong(expected, false))
    {
        auto& fftInputDataRef = audioProcessor.getFftData();
        auto numBins = size_t(MBRPAudioProcessor::fftSize / 2);

        // --- Создаем временный вектор для последних данных ---
        std::vector<float> latestDbData(numBins);
        // --------------------------------------------------

        // Проверяем и изменяем размер displayData, если нужно
        if (displayData.size() != numBins)
            displayData.resize(numBins);

        const float normalizationFactor = 100.0f * 2.0f / static_cast<float>(MBRPAudioProcessor::fftSize); // Используем ваш подобранный коэфф.

        float currentFramePeak = minDb; // Пик для *этого* кадра FFT

        // --- 1. Вычисляем последние уровни дБ ---
        for (size_t i = 0; i < numBins; ++i)
        {
            float magnitude = fftInputDataRef[i];
            float normalizedMagnitude = magnitude * normalizationFactor;
            latestDbData[i] = juce::Decibels::gainToDecibels(normalizedMagnitude, minDb);

            // Обновляем пик для текущего реального сигнала
            if (latestDbData[i] > currentFramePeak)
            {
                currentFramePeak = latestDbData[i];
            }
        }
        // Сохраняем реальный пик
        peakDbLevel.store(currentFramePeak);
        // ----------------------------------------

        // --- 2. Обновляем отображаемые данные с затуханием ---
        for (size_t i = 0; i < numBins; ++i)
        {
            float newVal = latestDbData[i];
            float oldVal = displayData[i];

            if (newVal > oldVal) // Атака: Увеличиваем мгновенно
            {
                displayData[i] = newVal;
            }
            else // Затухание: Уменьшаем плавно
            {
                // Умножаем на коэффициент затухания
                float oldGain = juce::Decibels::decibelsToGain(oldVal);
                float decayedGain = oldGain * decayFactor;
                float decayedDb = juce::Decibels::gainToDecibels(decayedGain, minDb);
                displayData[i] = std::max(decayedDb, newVal);
            }
        }
        // --------------------------------------------------

        repaint(); // Запрашиваем перерисовку с новыми displayData
    }
    else // Если новых данных FFT нет
    {
        // --- 3. Применяем затухание, даже если нет новых данных ---
        bool needsRepaint = false;
        for (size_t i = 0; i < displayData.size(); ++i)
        {
            float oldVal = displayData[i];
            if (oldVal > minDb) // Затухаем, только если есть что затухать
            {
                float oldGain = juce::Decibels::decibelsToGain(oldVal);
                float decayedGain = oldGain * decayFactor;
                float decayedDb = juce::Decibels::gainToDecibels(decayedGain, minDb);
                // Сравниваем с предыдущим значением, чтобы определить, нужна ли перерисовка
                if (decayedDb < oldVal)
                {
                    displayData[i] = decayedDb;
                    needsRepaint = true;
                }
                else // Если затухание не изменило значение (уперлись в minDb), можно остановить
                {
                    displayData[i] = minDb; // Устанавливаем точно в minDb
                    if (!juce::approximatelyEqual(oldVal, minDb)) needsRepaint = true; // Перерисовать, если было не minDb
                }
            }
        }
        // Перерисовываем, только если значения действительно изменились из-за затухания
        if (needsRepaint)
        {
            repaint();
        }
        // ------------------------------------------------------
    }
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
    auto right = bounds.getRight(); // Добавим правую границу для удобства
    auto numBins = displayData.size();
    auto sampleRate = audioProcessor.getSampleRate();

    if (numBins == 0 || sampleRate <= 0 || width <= 0) return; // Безопасная проверка

    // --- Собираем точки для отрисовки ---
    // Сохраняем только точки в видимом диапазоне частот
    std::vector<Point<float>> points;
    points.reserve(numBins); // Предварительное выделение памяти

    // Добавляем начальную точку на левой границе внизу
    points.push_back({ left, bottom });

    for (size_t i = 1; i < numBins; ++i)
    {
        float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);

        // Ограничиваем частоты видимой областью + небольшой запас
        if (freq >= minFreq * 0.9f && freq <= maxFreq * 1.1f)
        {
            float x = left + frequencyToX(freq, width);
            // --- ИЗМЕНЕНО: Используем displayData ---
            float dbValue = displayData[i];
            // --------------------------------------
            float y = jmap(dbValue, minDb, maxDb, bottom, top);
            x = jlimit(left, right, x);
            y = jlimit(top, y, bottom);
            points.push_back({ x, y });
        }
        // Прерываем цикл, если вышли за правую границу (оптимизация)
        if (freq > maxFreq * 1.1f)
            break;
    }

    // Добавляем конечную точку на правой границе внизу
    points.push_back({ right, bottom });

    if (points.size() < 2) return; // Нечего рисовать

    // --- Рисуем основную кривую с использованием cubicTo ---
    Path spectrumPath;
    spectrumPath.startNewSubPath(points[0]); // Начинаем с первой точки (left, bottom)

    for (size_t i = 1; i < points.size(); ++i)
    {
        // Предыдущая точка
        const auto& p0 = points[i - 1];
        // Текущая точка (конечная для сегмента)
        const auto& p1 = points[i];

        // Контрольные точки для кубической кривой Безье
        // cp1 контролирует кривизну *после* p0
        // cp2 контролирует кривизну *перед* p1
        // Простой способ: разместить их горизонтально между точками
        Point<float> cp1, cp2;
        float midpointX = (p0.x + p1.x) * 0.5f;
        cp1.x = midpointX;
        cp1.y = p0.y; // Вертикально на уровне предыдущей точки
        cp2.x = midpointX;
        cp2.y = p1.y; // Вертикально на уровне текущей точки

        // Добавляем кубический сегмент
        spectrumPath.cubicTo(cp1, cp2, p1);
    }

    // --- Заливка под кривой (опционально) ---
    // Path должна быть замкнута для корректной заливки (мы добавили точки на bottom)
    ColourGradient fillGradient(spectrumLineColour.withAlpha(0.4f), left, bottom,
        spectrumLineColour.darker(0.8f).withAlpha(0.1f), left, top, false);
    g.setGradientFill(fillGradient);
    g.fillPath(spectrumPath); // Заливаем сформированный путь

    // --- Обводка кривой ---
    g.setColour(spectrumLineColour);
    g.strokePath(spectrumPath, PathStrokeType(1.5f));


    // --- Рисуем красную кривую для значений выше 0 дБ (Clipping) ---
    // Логика остается похожей, но используем cubicTo вместо lineTo
    Path overZeroPath;
    bool pathStarted = false;
    float zeroDbY = jmap(0.0f, minDb, maxDb, bottom, top);
    zeroDbY = jlimit(top, zeroDbY, bottom); // Ограничиваем Y для 0 дБ

    // Нам нужны исходные dB значения, чтобы точно найти пересечение с 0 дБ
    std::vector<Point<float>> pointsAboveZero; // Точки для красной кривой

    for (size_t i = 1; i < numBins; ++i) // Снова итерируем по бинам
    {
        float freq = static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);
        if (freq < minFreq || freq > maxFreq) continue;

        float currentDb = displayData[i];
        float x = left + frequencyToX(freq, width);
        x = jlimit(left, right, x); // Ограничиваем X

        if (currentDb >= 0.0f) // Если точка выше или на 0 дБ
        {
            float y = jmap(currentDb, minDb, maxDb, bottom, top);
            y = jlimit(top, y, bottom); // Ограничиваем Y

            if (!pathStarted) // Начинаем новый сегмент
            {
                // Находим предыдущую точку (если она была ниже 0, нужна точка пересечения)
                float prevDb = (i > 1) ? displayData[i - 1] : minDb;
                if (prevDb < 0.0f && i > 1)
                {
                    // Найдем точку пересечения с 0 дБ (приблизительно)
                    float prevFreq = static_cast<float>(i - 1) * static_cast<float>(sampleRate) / static_cast<float>(MBRPAudioProcessor::fftSize);
                    float prevX = left + frequencyToX(prevFreq, width);
                    prevX = jlimit(left, right, prevX);
                    // Добавляем точку на уровне 0 дБ
                    pointsAboveZero.push_back({ prevX, zeroDbY });
                }
                // Добавляем текущую точку (которая выше 0 дБ)
                pointsAboveZero.push_back({ x, y });
                pathStarted = true;
            }
            else // Продолжаем сегмент
            {
                pointsAboveZero.push_back({ x, y });
            }
        }
        else // Точка ниже 0 дБ
        {
            if (pathStarted) // Заканчиваем сегмент
            {
                // Добавляем последнюю точку на уровне 0 дБ
                pointsAboveZero.push_back({ x, zeroDbY });

                // Рисуем кривую для собранных точек
                if (pointsAboveZero.size() >= 2)
                {
                    overZeroPath.startNewSubPath(pointsAboveZero[0]);
                    for (size_t p_idx = 1; p_idx < pointsAboveZero.size(); ++p_idx)
                    {
                        const auto& p0 = pointsAboveZero[p_idx - 1];
                        const auto& p1 = pointsAboveZero[p_idx];
                        Point<float> cp1, cp2;
                        float midpointX = (p0.x + p1.x) * 0.5f;
                        cp1.x = midpointX; cp1.y = p0.y;
                        cp2.x = midpointX; cp2.y = p1.y;
                        overZeroPath.cubicTo(cp1, cp2, p1);
                    }
                    g.setColour(overZeroDbColour);
                    g.strokePath(overZeroPath, PathStrokeType(1.5f));
                }
                overZeroPath.clear();
                pointsAboveZero.clear();
                pathStarted = false;
            }
        }
    }
    // Если вышли из цикла, а сегмент был активен
    if (pathStarted && pointsAboveZero.size() >= 2)
    {
        // Добавляем конечную точку на правой границе на 0 дБ? Или просто рисуем то, что есть.
        // pointsAboveZero.push_back({right, zeroDbY});
        overZeroPath.startNewSubPath(pointsAboveZero[0]);
        for (size_t p_idx = 1; p_idx < pointsAboveZero.size(); ++p_idx)
        {
            const auto& p0 = pointsAboveZero[p_idx - 1];
            const auto& p1 = pointsAboveZero[p_idx];
            Point<float> cp1, cp2;
            float midpointX = (p0.x + p1.x) * 0.5f;
            cp1.x = midpointX; cp1.y = p0.y;
            cp2.x = midpointX; cp2.y = p1.y;
            overZeroPath.cubicTo(cp1, cp2, p1);
        }
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