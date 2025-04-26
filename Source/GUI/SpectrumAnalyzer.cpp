#include "SpectrumAnalyzer.h"
#include "Utilities.h"
#include "LookAndFeel.h"

namespace MBRP_GUI
{
    // --- Реализация SpectrumAnalyzer ---
    SpectrumAnalyzer::SpectrumAnalyzer(MBRPAudioProcessor& processor, // Принимаем MBRPAudioProcessor
        SimpleFifo& leftFifo,
        SimpleFifo& rightFifo) :
        audioProcessor(processor), // Сохраняем ссылку на процессор
        sampleRate(processor.getSampleRate()), // Получаем начальный sampleRate
        leftPathProducer(leftFifo), // Инициализируем PathProducers
        rightPathProducer(rightFifo)
    {
        leftPathProducer.updateNegativeInfinity(NEG_INFINITY);
        rightPathProducer.updateNegativeInfinity(NEG_INFINITY);
        startTimerHz(60);
    }

    void SpectrumAnalyzer::timerCallback()
    {
        if (shouldShowFFTAnalysis) {
            auto bounds = getLocalBounds();
            auto fftBounds = SpectrumAnalyzerUtils::getAnalysisArea(bounds).toFloat();
            sampleRate = audioProcessor.getSampleRate();
            leftPathProducer.process(fftBounds, sampleRate);
            rightPathProducer.process(fftBounds, sampleRate);
        }
        repaint();
    }

    void SpectrumAnalyzer::paint(juce::Graphics& g)
    {
        using namespace juce;
        g.fillAll(Colours::black);
        auto bounds = getLocalBounds();
        drawBackgroundGrid(g, bounds);
        drawTextLabels(g, bounds);
        if (shouldShowFFTAnalysis) {
            drawFFTAnalysis(g, bounds);
        }
    }

    void SpectrumAnalyzer::resized()
    {
        using namespace juce;
        auto bounds = getLocalBounds();
        auto fftBounds = SpectrumAnalyzerUtils::getAnalysisArea(bounds).toFloat();
        auto negInf = mapY(NEG_INFINITY, fftBounds.getBottom(), fftBounds.getY());
        leftPathProducer.updateNegativeInfinity(negInf);
        rightPathProducer.updateNegativeInfinity(negInf);
    }

    void SpectrumAnalyzer::drawFFTAnalysis(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        auto analysisArea = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        Graphics::ScopedSaveState state(g); // Сохраняем состояние графики
        g.reduceClipRegion(analysisArea); // Ограничиваем рисование этой областью

        auto leftPath = leftPathProducer.getPath();
        auto rightPath = rightPathProducer.getPath();
        // --- ОТЛАДКА ПОЛУЧЕНИЯ ПУТИ ---
        DBG("DRAW FFT: Left Path is empty = " << (leftPath.isEmpty() ? "Yes" : "No")
            << ", Right Path is empty = " << (rightPath.isEmpty() ? "Yes" : "No"));
        // -------------------------------

        leftPath.applyTransform(AffineTransform().translation(analysisArea.getX(), 0));
        g.setColour(ColorScheme::getInputSignalColor());
        g.strokePath(leftPath, PathStrokeType(1.f));

        rightPath.applyTransform(AffineTransform().translation(analysisArea.getX(), 0));
        g.setColour(ColorScheme::getOutputSignalColor());
        g.strokePath(rightPath, PathStrokeType(1.f));
    }

    // --- Реализация вспомогательных функций рисования ---

    std::vector<float> SpectrumAnalyzer::getFrequencies()
    {
        // Возвращаем стандартный набор частот для сетки/меток
        return { 20, /*30, 40,*/ 50, 100, 200, /*300, 400,*/ 500, 1000, 2000, /*3000, 4000,*/ 5000, 10000, 20000 };
    }

    std::vector<float> SpectrumAnalyzer::getGains()
    {
        // Генерируем уровни для сетки/меток в нашем диапазоне dB
        std::vector<float> values;
        float increment = 12.f; // Шаг основной сетки
        // Добавляем кратные шагу от верха вниз
        for (auto db = MAX_DB; db >= NEG_INFINITY; db -= increment)
        {
            values.push_back(db);
        }
        // Убедимся, что 0 dB и нижняя граница присутствуют
        if (std::find_if(values.begin(), values.end(), [](float db) { return std::abs(db) < 0.01f; }) == values.end())
            values.push_back(0.0f);
        if (values.back() > NEG_INFINITY + 0.1f)
            values.push_back(NEG_INFINITY);

        // Можем добавить промежуточные деления (например, кратные 6)
        for (auto db = MAX_DB - increment / 2.0f; db >= NEG_INFINITY; db -= increment)
        {
            if (std::find_if(values.begin(), values.end(), [db](float v) { return std::abs(v - db) < 0.01f; }) == values.end())
                values.push_back(db);
        }

        std::sort(values.begin(), values.end()); // Сортируем для правильного отображения
        return values;
    }

    std::vector<float> SpectrumAnalyzer::getXs(const std::vector<float>& freqs, float left, float width)
    {
        // Вычисляем X-координаты для заданных частот
        std::vector<float> xs;
        for (auto f : freqs)
        {
            // Используем mapX из Utilities
            xs.push_back(mapX(f, juce::Rectangle<float>(left, 0, width, 0)));
        }
        return xs;
    }

    void SpectrumAnalyzer::drawBackgroundGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        auto freqs = getFrequencies();
        auto analysisArea = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        auto left = analysisArea.getX();
        auto right = analysisArea.getRight();
        auto top = analysisArea.getY();
        auto bottom = analysisArea.getBottom();
        auto width = analysisArea.getWidth();

        auto xs = getXs(freqs, left, width);
        g.setColour(ColorScheme::getAnalyzerGridColor()); // Цвет сетки

        // Рисуем вертикальные линии
        for (auto x : xs) { g.drawVerticalLine(roundToInt(x), top, bottom); }

        auto gains = getGains();
        // Рисуем горизонтальные линии
        for (auto gainDb : gains)
        {
            // Используем mapY из Utilities
            auto y = mapY(gainDb, (float)bottom, (float)top);
            // Выделяем линию 0 дБ цветом
            bool isZeroDb = std::abs(gainDb) < 0.01f;
            // Делаем основные деления (кратные 12 или 0) чуть заметнее
            bool isMajorTick = isZeroDb || (int(gainDb) % 12 == 0);
            g.setColour(isZeroDb ? ColorScheme::getSliderRangeTextColor().withAlpha(0.75f)
                : (isMajorTick ? ColorScheme::getAnalyzerGridColor() : ColorScheme::getTickColor())); // Цвет линии
            g.drawHorizontalLine(roundToInt(y), left, right);
        }
    }

    void SpectrumAnalyzer::drawTextLabels(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        g.setColour(ColorScheme::getScaleTextColor()); // Цвет текста меток
        const int fontHeight = 10;
        g.setFont(fontHeight);

        auto analysisArea = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        auto left = analysisArea.getX() + 1;
        auto top = analysisArea.getY();
        auto bottom = analysisArea.getBottom();
        auto width = analysisArea.getWidth();
        auto right = analysisArea.getRight();

        auto freqs = getFrequencies();
        auto xs = getXs(freqs, left, width);

        // Рисуем метки частот под сеткой
        for (size_t i = 0; i < freqs.size(); ++i)
        {
            auto f = freqs[i];
            auto x = xs[i];
            bool addK = false;
            String str;
            if (f > 999.f) { addK = true; f /= 1000.f; }
            str << f; if (addK) str << "k";
            // str << "Hz"; // Можно убрать Hz для компактности

            auto textWidth = g.getCurrentFont().getStringWidth(str);
            Rectangle<int> r;
            r.setSize(textWidth, fontHeight);
            r.setCentre(roundToInt(x), bottom + fontHeight / 2 + 2); // Позиция под сеткой
            // Рисуем, только если полностью помещается
            if (r.getX() >= analysisArea.getX() && r.getRight() <= analysisArea.getRight())
                g.drawFittedText(str, r, juce::Justification::centred, 1);
        }

        auto gain = getGains();
        // Рисуем метки уровней слева и справа от сетки
        for (auto gDb : gain)
        {
            // Показываем метки для основных делений
            if (std::abs(gDb) < 0.01f || (int(gDb) != 0 && int(gDb) % 12 == 0) || gDb == MAX_DB || gDb == NEG_INFINITY)
            {
                auto y = mapY(gDb, (float)bottom, (float)top); // Используем mapY из Utilities
                String str;
                if (gDb > 0) str << "+";
                str << roundToInt(gDb);

                auto textWidth = g.getCurrentFont().getStringWidth(str);
                Rectangle<int> r;
                r.setSize(textWidth, fontHeight);

                // Метка справа
                r.setX(right + 4); // Отступ справа
                r.setCentre(r.getCentreX(), roundToInt(y));
                if (r.getY() >= top && r.getBottom() <= bottom) // Проверка по высоте
                    g.drawFittedText(str, r, juce::Justification::centredLeft, 1);

                // Метка слева
                r.setX(left - textWidth - 4); // Отступ слева
                r.setCentre(r.getCentreX(), roundToInt(y));
                if (r.getY() >= top && r.getBottom() <= bottom) // Проверка по высоте
                    g.drawFittedText(str, r, juce::Justification::centredRight, 1);
            }
        }
    }

    // --- Реализация AnalyzerOverlay ---
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
        juce::AudioParameterFloat& midXover) :
        lowMidXoverParam(lowXover),
        midHighXoverParam(midXover)
    {
        setInterceptsMouseClicks(false, false); // Оверлей не должен перехватывать клики
        startTimerHz(30); // Таймер для плавной перерисовки линий
    }

    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        // Рисуем линии кроссовера поверх анализатора
        drawCrossoverLines(g, getLocalBounds());
    }

    void AnalyzerOverlay::timerCallback()
    {
        // Просто запрашиваем перерисовку, чтобы линии обновлялись
        repaint();
    }

    void AnalyzerOverlay::drawCrossoverLines(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace juce;
        // Используем ту же область, что и анализатор
        bounds = SpectrumAnalyzerUtils::getAnalysisArea(bounds);
        const float top = static_cast<float>(bounds.getY());
        const float bottom = static_cast<float>(bounds.getBottom());

        // Получаем координаты X для текущих значений параметров
        float lowMidX = mapX(lowMidXoverParam.get(), bounds.toFloat()); // Используем mapX
        float midHighX = mapX(midHighXoverParam.get(), bounds.toFloat()); // Используем mapX

        g.setColour(Colours::lightblue.withAlpha(0.7f)); // Цвет линий
        float lineThickness = 1.5f;

        // Рисуем линии, если они попадают в видимую область
        if (lowMidX >= bounds.getX() && lowMidX <= bounds.getRight())
            g.drawVerticalLine(roundToInt(lowMidX), top, bottom);
        if (midHighX >= bounds.getX() && midHighX <= bounds.getRight())
            g.drawVerticalLine(roundToInt(midHighX), top, bottom);

        // Опционально: добавить "ручки" или маркеры на линиях
        // g.setColour(Colours::yellow);
        // g.fillEllipse(lowMidX - 3, top, 6, 6);
        // g.fillEllipse(midHighX - 3, top, 6, 6);
    }

} // Конец namespace MBRP_GUI