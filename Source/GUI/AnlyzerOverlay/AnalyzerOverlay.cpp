#include "AnalyzerOverlay.h"
#include "../Source/PluginProcessor.h" // Включаем, если нужны константы sampleRate и т.д.
                                     // но для данной реализации он не обязателен.
#include <juce_gui_basics/juce_gui_basics.h> // TextLayout, Graphics
#include <juce_dsp/juce_dsp.h>             // 
namespace MBRP_GUI
{

    // Конструктор
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
        juce::AudioParameterFloat& midXover) :
        lowMidXoverParam(lowXover),
        midHighXoverParam(midXover)
    {
        // Оверлей должен перехватывать события мыши, чтобы реагировать на клики/перетаскивания
        setInterceptsMouseClicks(true, true);
        setPaintingIsUnclipped(true);
        startTimerHz(30); // Таймер для перерисовки линий при изменении параметров
    }

    // Отрисовка
    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        // Рисуем только линии кроссовера, используя границы ГРАФИКА
        drawCrossoverLines(g, getGraphBounds());
    }

    // Таймер (просто перерисовывает)
    void AnalyzerOverlay::timerCallback()
    {
        // Перерисовываем, чтобы линии обновлялись, если параметры изменились извне
        repaint();
    }

    // Изменение размера (просто перерисовываем)
    void AnalyzerOverlay::resized()
    {
        repaint();
    }

    // --- Получение области графика ---
    // ВАЖНО: Эта логика ДОЛЖНА СОВПАДАТЬ с тем, как SpectrumAnalyzer определяет свою область
    juce::Rectangle<float> AnalyzerOverlay::getGraphBounds() const
    {
        // Пример логики, как было в SpectrumAnalyzer::drawGrid
        // Замените на актуальную логику вашего SpectrumAnalyzer, если она отличается!
        return getLocalBounds().toFloat().reduced(1.f, 5.f);
    }


    // --- Преобразование X в частоту ---
    float AnalyzerOverlay::xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const
    {
        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();

        // Используем mapXToFreqLog из хедера, используя КОНСТАНТЫ КЛАССА minLogFreq/maxLogFreq
        float freq = mapXToFreqLog(x, left, width, minLogFreq, maxLogFreq);

        // Дополнительно ограничиваем частоту, чтобы она не выходила
        // за пределы допустимых значений ПАРАМЕТРОВ и ГЛОБАЛЬНЫХ констант.

        // Убедимся, что частота не ниже глобального минимума и не выше максимального значения параметра Mid/High
        freq = juce::jlimit(minLogFreq,                                  // Используем константу класса
            midHighXoverParam.getNormalisableRange().end,  // Максимум из параметра Mid/High
            freq);

        // Убедимся, что частота не ниже минимального значения параметра Low/Mid и не выше глобального максимума
        freq = juce::jlimit(lowMidXoverParam.getNormalisableRange().start, // Минимум из параметра Low/Mid
            maxLogFreq,                                  // Используем константу класса
            freq);

        // Финальная проверка на глобальные пределы (может быть избыточна, но безопасна)
        freq = juce::jlimit(minLogFreq, maxLogFreq, freq);

        return freq;
    }

    // --- Отрисовка линий кроссовера ---
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

        // Используем mapFreqToXLog из хедера
        float lowMidX = mapFreqToXLog(lowMidFreq, left, width, minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighFreq, left, width, minLogFreq, maxLogFreq);

        // Рисуем линии
        g.setColour(crossoverLineColour.withAlpha(0.7f));
        if (lowMidX >= left && lowMidX <= right) g.drawVerticalLine(roundToInt(lowMidX), top, bottom);

        g.setColour(crossoverLineColour2.withAlpha(0.7f));
        if (midHighX >= left && midHighX <= right) g.drawVerticalLine(roundToInt(midHighX), top, bottom);

        // Рисуем ручки
        const float handleSize = 6.0f;
        const float handleRadius = handleSize / 2.0f;
        g.setColour(crossoverLineColour);
        if (lowMidX >= left && lowMidX <= right) g.fillEllipse(lowMidX - handleRadius, top, handleSize, handleSize);
        g.setColour(crossoverLineColour2);
        if (midHighX >= left && midHighX <= right) g.fillEllipse(midHighX - handleRadius, top, handleSize, handleSize);
    }

    // --- Обработчики мыши ---
    void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    {
        auto graphBounds = getGraphBounds(); // Получаем область графика
        if (!graphBounds.contains(event.getPosition().toFloat())) // Клик вне области графика
        {
            currentDraggingState = DraggingState::None;
            return;
        }

        float mouseX = static_cast<float>(event.x);

        // Получаем текущие X-координаты линий внутри области графика
        float lowMidX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

        // Проверяем клик рядом с линиями
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
        else { // Клик по области
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
        repaint(); // Обновить вид (например, курсор)
    }

    void AnalyzerOverlay::mouseDrag(const juce::MouseEvent& event)
    {
        if (currentDraggingState == DraggingState::None) return;

        auto graphBounds = getGraphBounds();
        // Ограничиваем X координату границами графика перед конвертацией в частоту
        float mouseX = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), (float)event.x);
        float newFreq = xToFrequency(mouseX, graphBounds);

        if (currentDraggingState == DraggingState::DraggingLowMid) {
            newFreq = juce::jlimit(minLogFreq, midHighXoverParam.get() - 1.0f, newFreq); // Ограничение
            lowMidXoverParam.setValueNotifyingHost(lowMidXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            newFreq = juce::jlimit(lowMidXoverParam.get() + 1.0f, maxLogFreq, newFreq); // Ограничение
            midHighXoverParam.setValueNotifyingHost(midHighXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }
        // Перерисовка будет по таймеру или можно добавить repaint()
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