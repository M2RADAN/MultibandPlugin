#include "AnalyzerOverlay.h"
#include "../Source/PluginProcessor.h" // Для доступа к параметрам, если потребуется, но лучше через конструктор
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace MBRP_GUI
{
    // Конструктор
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowMidXover,
        juce::AudioParameterFloat& midXover,
        juce::AudioParameterFloat& midHighXover) :
        lowMidXoverParam(lowMidXover),
        midXoverParam(midXover),
        midHighXoverParam(midHighXover)
    {
        setInterceptsMouseClicks(true, true); // Перехватываем клики для себя и для детей (если есть)
        setPaintingIsUnclipped(true);         // Позволяет рисовать за пределами компонента (для ручек)
        startTimerHz(30); // Частота обновления для анимации
    }

    // Отрисовка
    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        auto graphBounds = getGraphBounds();
        drawHoverHighlight(g, graphBounds); // Подсветка под линиями
        drawCrossoverLines(g, graphBounds); // Линии поверх
    }

    // Таймер для анимации
    void AnalyzerOverlay::timerCallback()
    {
        bool needsRepaint = false;

        if (!juce::approximatelyEqual(currentHighlightAlpha, targetHighlightAlpha)) {
            currentHighlightAlpha += (targetHighlightAlpha - currentHighlightAlpha) * alphaAnimationSpeed;
            if (std::abs(currentHighlightAlpha - targetHighlightAlpha) < 0.001f) {
                currentHighlightAlpha = targetHighlightAlpha;
            }
            needsRepaint = true;
        }

        // Проверка на изменение параметров кроссовера извне (если нужно, чтобы линии двигались без мыши)
        // Это можно оптимизировать, кешируя последние значения частот.
        // For now, always repaint to ensure lines are correct if parameters change via automation.
        needsRepaint = true;

        if (needsRepaint) {
            repaint();
        }
    }

    void AnalyzerOverlay::resized()
    {
        repaint(); // Перерисовать при изменении размера
    }

    juce::Rectangle<float> AnalyzerOverlay::getGraphBounds() const
    {
        // Эта область должна точно совпадать с областью рисования графика в SpectrumAnalyzer
        return getLocalBounds().toFloat().reduced(1.f, 5.f); // Примерные значения
    }

    float AnalyzerOverlay::xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const
    {
        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();
        // Используем функцию маппинга из .h файла
        float freq = mapXToFreqLog(x, left, width, minLogFreq, maxLogFreq);
        // Ограничиваем частоту общим диапазоном анализатора
        freq = juce::jlimit(minLogFreq, maxLogFreq, freq);
        return freq;
    }

    void AnalyzerOverlay::drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        using namespace juce;
        auto width = graphBounds.getWidth();
        auto top = graphBounds.getY();
        auto bottom = graphBounds.getBottom();
        auto left = graphBounds.getX();
        auto right = graphBounds.getRight();

        // Получаем текущие значения частот кроссоверов
        float lmFreq = lowMidXoverParam.get();
        float mFreq = midXoverParam.get();
        float mhFreq = midHighXoverParam.get();

        // Преобразуем частоты в X-координаты
        float lmX = mapFreqToXLog(lmFreq, left, width, minLogFreq, maxLogFreq);
        float mX = mapFreqToXLog(mFreq, left, width, minLogFreq, maxLogFreq);
        float mhX = mapFreqToXLog(mhFreq, left, width, minLogFreq, maxLogFreq);

        // Цвета для линий (из ColorScheme)
        const auto lmCol = ColorScheme::getOrangeBorderColor(); // Low/LowMid
        const auto mCol = ColorScheme::getMidBandColor();     // LowMid/MidHigh (новый цвет или существующий)
        const auto mhCol = ColorScheme::getMidHighCrossoverColor(); // MidHigh/High

        // Рисуем линии
        g.setColour(lmCol.withAlpha(0.7f));
        if (lmX >= left && lmX <= right) g.drawVerticalLine(roundToInt(lmX), top, bottom);

        g.setColour(mCol.withAlpha(0.7f));
        if (mX >= left && mX <= right) g.drawVerticalLine(roundToInt(mX), top, bottom);

        g.setColour(mhCol.withAlpha(0.7f));
        if (mhX >= left && mhX <= right) g.drawVerticalLine(roundToInt(mhX), top, bottom);

        // Рисуем "ручки" сверху линий
        const float handleSize = 8.0f; // Увеличил размер ручки
        const float handleRadius = handleSize / 2.0f;

        auto drawHandle = [&](float xPos, Colour col) {
            if (xPos >= left && xPos <= right) {
                g.setColour(col);
                // Рисуем ручку так, чтобы она была центрирована на линии и немного выступала вверх
                g.fillEllipse(xPos - handleRadius, top - handleRadius, handleSize, handleSize);
                g.setColour(col.darker(0.3f));
                g.drawEllipse(xPos - handleRadius, top - handleRadius, handleSize, handleSize, 1.0f);
            }
        };

        drawHandle(lmX, lmCol);
        drawHandle(mX, mCol);
        drawHandle(mhX, mhCol);
    }

    void AnalyzerOverlay::drawHoverHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        if (currentHighlightAlpha <= 0.0f) return; // Не рисуем, если прозрачно

        using namespace juce;
        float targetX = -1.0f;
        Colour highlightColour = Colours::transparentBlack;

        HoverState stateToUse = HoverState::None;
        if (currentDraggingState != DraggingState::None) {
            if (currentDraggingState == DraggingState::DraggingLowMid) stateToUse = HoverState::HoveringLowMid;
            else if (currentDraggingState == DraggingState::DraggingMid) stateToUse = HoverState::HoveringMid;
            else if (currentDraggingState == DraggingState::DraggingMidHigh) stateToUse = HoverState::HoveringMidHigh;
        }
        else if (currentHoverState != HoverState::None) {
            stateToUse = currentHoverState;
        }
        else {
            stateToUse = lastHoverStateForColor;
        }

        if (stateToUse == HoverState::None) return;

        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();
        auto top = graphBounds.getY();
        auto bottom = graphBounds.getBottom();

        if (stateToUse == HoverState::HoveringLowMid) {
            targetX = mapFreqToXLog(lowMidXoverParam.get(), left, width, minLogFreq, maxLogFreq);
            highlightColour = ColorScheme::getOrangeBorderColor();
        }
        else if (stateToUse == HoverState::HoveringMid) {
            targetX = mapFreqToXLog(midXoverParam.get(), left, width, minLogFreq, maxLogFreq);
            highlightColour = ColorScheme::getMidBandColor();
        }
        else { // HoveringMidHigh
            targetX = mapFreqToXLog(midHighXoverParam.get(), left, width, minLogFreq, maxLogFreq);
            highlightColour = ColorScheme::getMidHighCrossoverColor();
        }

        if (targetX >= left && targetX <= graphBounds.getRight())
        {
            Rectangle<float> highlightRect;
            highlightRect.setWidth(highlightRectWidth);
            highlightRect.setCentre(targetX, graphBounds.getCentreY());
            highlightRect.setY(top);
            highlightRect.setBottom(bottom);

            g.setColour(highlightColour.withAlpha(currentHighlightAlpha));
            g.fillRect(highlightRect);
        }
    }

    void AnalyzerOverlay::mouseMove(const juce::MouseEvent& event)
    {
        if (currentDraggingState != DraggingState::None) return; // Игнорируем, если идет драг

        auto graphBounds = getGraphBounds();
        HoverState newState = HoverState::None;
        float newTargetAlpha = 0.0f;
        juce::MouseCursor cursor = juce::MouseCursor::NormalCursor;

        if (graphBounds.contains(event.getPosition().toFloat()))
        {
            float mouseX = static_cast<float>(event.x);
            float lmX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mX = mapFreqToXLog(midXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mhX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

            // Проверяем, находится ли X координата мыши близко к одной из линий
            if (std::abs(mouseX - lmX) < dragTolerance) {
                newState = HoverState::HoveringLowMid;
            }
            else if (std::abs(mouseX - mX) < dragTolerance) {
                newState = HoverState::HoveringMid;
            }
            else if (std::abs(mouseX - mhX) < dragTolerance) {
                newState = HoverState::HoveringMidHigh;
            }


            if (newState != HoverState::None) {
                newTargetAlpha = targetAlphaValue;
                cursor = juce::MouseCursor::LeftRightResizeCursor;
            }
        }
        setMouseCursor(cursor);

        if (newState != HoverState::None) {
            lastHoverStateForColor = newState;
        }
        if (newState != currentHoverState) {
            currentHoverState = newState;
        }
        if (!juce::approximatelyEqual(newTargetAlpha, targetHighlightAlpha)) {
            targetHighlightAlpha = newTargetAlpha;
        }
    }

    void AnalyzerOverlay::mouseExit(const juce::MouseEvent& /*event*/)
    {
        if (currentDraggingState == DraggingState::None) {
            if (currentHoverState != HoverState::None) {
                lastHoverStateForColor = currentHoverState;
            }
            targetHighlightAlpha = 0.0f;
            currentHoverState = HoverState::None;
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    {
        auto graphBounds = getGraphBounds();
        DraggingState newDragState = DraggingState::None;
        juce::AudioParameterFloat* paramToChange = nullptr;

        float mouseX = static_cast<float>(event.x);
        float lmX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float mX = mapFreqToXLog(midXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float mhX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

        if (event.y >= graphBounds.getY() && event.y <= graphBounds.getBottom()) {
            if (std::abs(mouseX - lmX) < dragTolerance) {
                newDragState = DraggingState::DraggingLowMid;
                paramToChange = &lowMidXoverParam;
                currentHoverState = HoverState::HoveringLowMid; // Устанавливаем для подсветки
            }
            else if (std::abs(mouseX - mX) < dragTolerance) {
                newDragState = DraggingState::DraggingMid;
                paramToChange = &midXoverParam;
                currentHoverState = HoverState::HoveringMid; // Устанавливаем для подсветки
            }
            else if (std::abs(mouseX - mhX) < dragTolerance) {
                newDragState = DraggingState::DraggingMidHigh;
                paramToChange = &midHighXoverParam;
                currentHoverState = HoverState::HoveringMidHigh; // Устанавливаем для подсветки
            }
        }


        if (newDragState != DraggingState::None && paramToChange != nullptr) {
            currentDraggingState = newDragState;
            lastHoverStateForColor = currentHoverState;
            targetHighlightAlpha = targetAlphaValue;
            currentHighlightAlpha = targetAlphaValue; // Мгновенно показать
            paramToChange->beginChangeGesture();
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            repaint();
        }
        else if (graphBounds.contains(event.getPosition().toFloat())) { // Клик по области, не по ручке
            currentDraggingState = DraggingState::None;
            if (currentHoverState != HoverState::None) {
                lastHoverStateForColor = currentHoverState;
            }
            currentHoverState = HoverState::None;
            targetHighlightAlpha = 0.0f; // Затухание, если была подсветка
            setMouseCursor(juce::MouseCursor::NormalCursor);

            float clickedFreq = xToFrequency(mouseX, graphBounds);
            int bandIndex = 0;
            if (clickedFreq < lowMidXoverParam.get()) bandIndex = 0; // Low
            else if (clickedFreq < midXoverParam.get()) bandIndex = 1; // Low-Mid
            else if (clickedFreq < midHighXoverParam.get()) bandIndex = 2; // Mid-High
            else bandIndex = 3; // High

            if (onBandAreaClicked) {
                onBandAreaClicked(bandIndex);
            }
            DBG("Overlay: Clicked in band area: " << bandIndex << " (Freq: " << clickedFreq << ")");
            if (currentHighlightAlpha > 0.0f) repaint();
        }
        else { // Клик вне области графика
            currentDraggingState = DraggingState::None;
            targetHighlightAlpha = 0.0f;
            currentHoverState = HoverState::None;
            if (currentHighlightAlpha > 0.0f) repaint();
        }
    }

    void AnalyzerOverlay::mouseDrag(const juce::MouseEvent& event)
    {
        if (currentDraggingState == DraggingState::None) return;

        auto graphBounds = getGraphBounds();
        float mouseX = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), (float)event.x);
        float newFreqRaw = xToFrequency(mouseX, graphBounds); // Частота по X координате мыши

        juce::AudioParameterFloat* paramToUpdate = nullptr;
        float minAllowedFreq = minLogFreq; // Глобальный минимум
        float maxAllowedFreq = maxLogFreq; // Глобальный максимум


        if (currentDraggingState == DraggingState::DraggingLowMid) {
            paramToUpdate = &lowMidXoverParam;
            // Не может быть ниже глобального минимума и не может быть выше, чем midXover минус зазор
            minAllowedFreq = juce::jmax(minAllowedFreq, paramToUpdate->getNormalisableRange().start);
            maxAllowedFreq = juce::jmin(maxAllowedFreq, midXoverParam.get() - MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
            maxAllowedFreq = juce::jmin(maxAllowedFreq, paramToUpdate->getNormalisableRange().end);
        }
        else if (currentDraggingState == DraggingState::DraggingMid) {
            paramToUpdate = &midXoverParam;
            // Не может быть ниже, чем lowMidXover плюс зазор, и не выше, чем midHighXover минус зазор
            minAllowedFreq = juce::jmax(minAllowedFreq, lowMidXoverParam.get() + MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
            minAllowedFreq = juce::jmax(minAllowedFreq, paramToUpdate->getNormalisableRange().start);
            maxAllowedFreq = juce::jmin(maxAllowedFreq, midHighXoverParam.get() - MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
            maxAllowedFreq = juce::jmin(maxAllowedFreq, paramToUpdate->getNormalisableRange().end);
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            paramToUpdate = &midHighXoverParam;
            // Не может быть ниже, чем midXover плюс зазор, и не выше глобального максимума
            minAllowedFreq = juce::jmax(minAllowedFreq, midXoverParam.get() + MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
            minAllowedFreq = juce::jmax(minAllowedFreq, paramToUpdate->getNormalisableRange().start);
            maxAllowedFreq = juce::jmin(maxAllowedFreq, paramToUpdate->getNormalisableRange().end);
        }

        if (paramToUpdate) {
            float finalFreq = juce::jlimit(minAllowedFreq, maxAllowedFreq, newFreqRaw);
            paramToUpdate->setValueNotifyingHost(paramToUpdate->getNormalisableRange().convertTo0to1(finalFreq));
        }

        // Обновление курсора и состояния наведения во время драга
        HoverState hoverStateForUpdate = HoverState::None;
        juce::MouseCursor cursor = juce::MouseCursor::LeftRightResizeCursor; // По умолчанию для драга

        float currentEventMouseX = (float)event.x;
        float lmX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float mX = mapFreqToXLog(midXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float mhX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

        // Проверяем, находится ли мышь все еще над соответствующей линией (или ее ручкой)
        // Упрощенная проверка, можно уточнить по Y координате, если ручки маленькие
        if (currentDraggingState == DraggingState::DraggingLowMid && std::abs(currentEventMouseX - lmX) < dragTolerance * 2.f) { // Увеличил допуск для удержания
            hoverStateForUpdate = HoverState::HoveringLowMid;
        }
        else if (currentDraggingState == DraggingState::DraggingMid && std::abs(currentEventMouseX - mX) < dragTolerance * 2.f) {
            hoverStateForUpdate = HoverState::HoveringMid;
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh && std::abs(currentEventMouseX - mhX) < dragTolerance * 2.f) {
            hoverStateForUpdate = HoverState::HoveringMidHigh;
        }
        else {
            // Если мышь ушла слишком далеко от линии во время драга, можно оставить курсор LeftRightResize
            // или сменить на NormalCursor. Оставим LeftRightResize, т.к. драг все еще активен.
        }
        setMouseCursor(cursor); // Курсор меняется в mouseMove, если драг не активен.

        if (hoverStateForUpdate != currentHoverState) currentHoverState = hoverStateForUpdate;
        if (currentHoverState != HoverState::None) lastHoverStateForColor = currentHoverState;

        targetHighlightAlpha = targetAlphaValue;  // Подсветка активна во время драга
        // repaint(); // Таймер и так вызывает repaint
    }

    void AnalyzerOverlay::mouseUp(const juce::MouseEvent& event)
    {
        juce::AudioParameterFloat* paramToEndGesture = nullptr;
        if (currentDraggingState == DraggingState::DraggingLowMid) paramToEndGesture = &lowMidXoverParam;
        else if (currentDraggingState == DraggingState::DraggingMid) paramToEndGesture = &midXoverParam;
        else if (currentDraggingState == DraggingState::DraggingMidHigh) paramToEndGesture = &midHighXoverParam;

        if (paramToEndGesture) {
            paramToEndGesture->endChangeGesture();
        }

        // --- ИЗМЕНЕНИЕ: Логика lastHoverStateForColor после mouseUp ---
        // Определяем, над какой линией находится мышь в момент отпускания,
        // чтобы подсветка осталась на ней или начала затухать с правильным цветом.
        HoverState hoverAtMouseUp = HoverState::None;
        auto graphBounds = getGraphBounds();
        if (graphBounds.contains(event.getPosition().toFloat())) {
            float mouseX = static_cast<float>(event.x);
            float lmX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mX = mapFreqToXLog(midXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mhX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

            if (std::abs(mouseX - lmX) < dragTolerance) hoverAtMouseUp = HoverState::HoveringLowMid;
            else if (std::abs(mouseX - mX) < dragTolerance) hoverAtMouseUp = HoverState::HoveringMid;
            else if (std::abs(mouseX - mhX) < dragTolerance) hoverAtMouseUp = HoverState::HoveringMidHigh;
        }

        if (hoverAtMouseUp != HoverState::None) {
            lastHoverStateForColor = hoverAtMouseUp;
        }
        else { // Если мышь не над линией, используем состояние, которое было во время драга
            if (currentDraggingState == DraggingState::DraggingLowMid) lastHoverStateForColor = HoverState::HoveringLowMid;
            else if (currentDraggingState == DraggingState::DraggingMid) lastHoverStateForColor = HoverState::HoveringMid;
            else if (currentDraggingState == DraggingState::DraggingMidHigh) lastHoverStateForColor = HoverState::HoveringMidHigh;
        }
        // --- КОНЕЦ ИЗМЕНЕНИЯ ---

        currentDraggingState = DraggingState::None;
        mouseMove(event);
    }

}
