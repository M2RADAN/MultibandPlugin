// --- START OF FILE AnalyzerOverlay.cpp ---
#include "AnalyzerOverlay.h"
#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace MBRP_GUI
{

    // Конструктор
    AnalyzerOverlay::AnalyzerOverlay(juce::AudioParameterFloat& lowXover,
        juce::AudioParameterFloat& midXover) :
        lowMidXoverParam(lowXover),
        midHighXoverParam(midXover)
    {
        setInterceptsMouseClicks(true, true);
        setPaintingIsUnclipped(true);
        startTimerHz(60);
    }

    // Отрисовка
    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        auto graphBounds = getGraphBounds();
        // Рисуем подсветку ПОД линиями
        drawHoverHighlight(g, graphBounds);
        // Рисуем линии поверх
        drawCrossoverLines(g, graphBounds);
    }

    // Таймер
    void AnalyzerOverlay::timerCallback()
    {
        bool needsRepaint = false;

        // Анимация прозрачности
        if (!juce::approximatelyEqual(currentHighlightAlpha, targetHighlightAlpha)) {
            currentHighlightAlpha += (targetHighlightAlpha - currentHighlightAlpha) * alphaAnimationSpeed;
            if (std::abs(currentHighlightAlpha - targetHighlightAlpha) < 0.001f) {
                currentHighlightAlpha = targetHighlightAlpha;
            }
            needsRepaint = true; // Нужна перерисовка из-за изменения альфы
        }

        // Всегда перерисовываем, чтобы линии обновлялись, если параметр изменился извне
        // (Это можно оптимизировать, добавив проверку на изменение частот)
        needsRepaint = true;

        if (needsRepaint) {
            repaint();
        }
    }

    // Изменение размера
    void AnalyzerOverlay::resized()
    {
        repaint();
    }

    // Получение области графика
    juce::Rectangle<float> AnalyzerOverlay::getGraphBounds() const
    {
        // Убедитесь, что это совпадает с SpectrumAnalyzer!
        return getLocalBounds().toFloat().reduced(1.f, 5.f);
    }

    // Преобразование X в частоту
    float AnalyzerOverlay::xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const
    {
        // (Версия без ошибок)
        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();
        float freq = mapXToFreqLog(x, left, width, minLogFreq, maxLogFreq);
        freq = juce::jlimit(minLogFreq, midHighXoverParam.getNormalisableRange().end, freq);
        freq = juce::jlimit(lowMidXoverParam.getNormalisableRange().start, maxLogFreq, freq);
        freq = juce::jlimit(minLogFreq, maxLogFreq, freq);
        return freq;
    }

    // Отрисовка линий кроссовера
    void AnalyzerOverlay::drawCrossoverLines(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        // (Без изменений)
        using namespace juce;
        auto width = graphBounds.getWidth();
        auto top = graphBounds.getY();
        auto bottom = graphBounds.getBottom();
        auto left = graphBounds.getX();
        auto right = graphBounds.getRight();

        float lowMidFreq = lowMidXoverParam.get();
        float midHighFreq = midHighXoverParam.get();

        float lowMidX = mapFreqToXLog(lowMidFreq, left, width, minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighFreq, left, width, minLogFreq, maxLogFreq);

        const auto lowMidCol = ColorScheme::getOrangeBorderColor();
        const auto midHighCol = ColorScheme::getMidHighCrossoverColor();

        g.setColour(lowMidCol.withAlpha(0.7f));
        if (lowMidX >= left && lowMidX <= right) g.drawVerticalLine(roundToInt(lowMidX), top, bottom);
        g.setColour(midHighCol.withAlpha(0.7f));
        if (midHighX >= left && midHighX <= right) g.drawVerticalLine(roundToInt(midHighX), top, bottom);

        const float handleSize = 6.0f;
        const float handleRadius = handleSize / 2.0f;
        g.setColour(lowMidCol);
        if (lowMidX >= left && lowMidX <= right) g.fillEllipse(lowMidX - handleRadius, top, handleSize, handleSize);
        g.setColour(midHighCol);
        if (midHighX >= left && midHighX <= right) g.fillEllipse(midHighX - handleRadius, top, handleSize, handleSize);
    }

    // Отрисовка подсветки
    void AnalyzerOverlay::drawHoverHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        if (currentHighlightAlpha <= 0.0f) {
            return; // Не рисуем, если полностью прозрачно
        }

        using namespace juce;
        float targetX = -1.0f;
        Colour highlightColour = Colours::transparentBlack;

        // Определяем, состояние для определения ПОЗИЦИИ и ЦВЕТА
        HoverState stateToUse = HoverState::None;

        if (currentDraggingState != DraggingState::None) {
            // Во время драга используем состояние драга
            stateToUse = (currentDraggingState == DraggingState::DraggingLowMid) ? HoverState::HoveringLowMid : HoverState::HoveringMidHigh;
        }
        else if (currentHoverState != HoverState::None) {
            // Если не драг, но есть наведение, используем его
            stateToUse = currentHoverState;
        }
        else {
            // Если нет ни драга, ни наведения, НО альфа > 0 (идет затухание),
            // используем ПОСЛЕДНЕЕ запомненное состояние для цвета и позиции
            stateToUse = lastHoverStateForColor;
        }

        // Если даже последнее состояние было None, рисовать нечего
        if (stateToUse == HoverState::None) {
            return;
        }

        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();
        auto top = graphBounds.getY();
        auto bottom = graphBounds.getBottom();

        // Получаем X и цвет для нужного состояния (stateToUse)
        if (stateToUse == HoverState::HoveringLowMid) {
            targetX = mapFreqToXLog(lowMidXoverParam.get(), left, width, minLogFreq, maxLogFreq);
            highlightColour = ColorScheme::getOrangeBorderColor();
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

            // Рисуем с текущей анимированной альфой
            g.setColour(highlightColour.withAlpha(currentHighlightAlpha));
            g.fillRect(highlightRect);
        }
    }

    // Обработчик движения мыши (когда кнопка НЕ нажата)
    void AnalyzerOverlay::mouseMove(const juce::MouseEvent& event)
    {
        if (event.mods.isLeftButtonDown() || currentDraggingState != DraggingState::None) {
            return; // Игнорируем, если идет драг или кнопка зажата без драга
        }

        auto graphBounds = getGraphBounds();
        HoverState newState = HoverState::None;
        float newTargetAlpha = 0.0f;
        juce::MouseCursor cursor = juce::MouseCursor::NormalCursor;

        if (graphBounds.contains(event.getPosition().toFloat()))
        {
            float mouseX = static_cast<float>(event.x);
            float lowMidX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float midHighX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

            if (std::abs(mouseX - lowMidX) < dragTolerance) {
                newState = HoverState::HoveringLowMid;
                newTargetAlpha = targetAlphaValue;
                cursor = juce::MouseCursor::LeftRightResizeCursor;
            }
            else if (std::abs(mouseX - midHighX) < dragTolerance) {
                newState = HoverState::HoveringMidHigh;
                newTargetAlpha = targetAlphaValue;
                cursor = juce::MouseCursor::LeftRightResizeCursor;
            }
        }
        setMouseCursor(cursor);

        // --- Обновление состояний ---
        // Запоминаем последнее активное состояние для цвета затухания
        if (newState != HoverState::None) {
            lastHoverStateForColor = newState;
        }

        // Обновляем текущее состояние и цель альфы, если они изменились
        if (newState != currentHoverState) {
            currentHoverState = newState;
        }
        if (!juce::approximatelyEqual(newTargetAlpha, targetHighlightAlpha)) {
            targetHighlightAlpha = newTargetAlpha; // Запускаем анимацию (появления или исчезновения)
        }
    }

    // Мышь покинула компонент
    void AnalyzerOverlay::mouseExit(const juce::MouseEvent& /*event*/)
    {
        if (currentDraggingState == DraggingState::None) {
            // Запоминаем последнее состояние перед тем, как сбросить текущее
            if (currentHoverState != HoverState::None) {
                lastHoverStateForColor = currentHoverState;
            }
            targetHighlightAlpha = 0.0f; // Запускаем затухание
            currentHoverState = HoverState::None;
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    // Нажатие мыши
    void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    {
        // ... (проверка graphBounds.contains) ...
        auto graphBounds = getGraphBounds();
        if (!graphBounds.contains(event.getPosition().toFloat()))
        {
            currentDraggingState = DraggingState::None;
            targetHighlightAlpha = 0.0f;
            // Не сбрасываем lastHoverStateForColor здесь
            currentHoverState = HoverState::None;
            return;
        }

        // ... (определение lowMidX, midHighX) ...
        float mouseX = static_cast<float>(event.x);
        float lowMidX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float midHighX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

        // ... (определение newDraggingState, initialHoverState, initialTargetAlpha) ...
        DraggingState newDraggingState = DraggingState::None;
        HoverState initialHoverState = HoverState::None;
        float initialTargetAlpha = 0.0f;

        if (std::abs(mouseX - lowMidX) < dragTolerance) {
            newDraggingState = DraggingState::DraggingLowMid;
            initialHoverState = HoverState::HoveringLowMid;
            initialTargetAlpha = targetAlphaValue;
            lowMidXoverParam.beginChangeGesture();
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        else if (std::abs(mouseX - midHighX) < dragTolerance) {
            newDraggingState = DraggingState::DraggingMidHigh;
            initialHoverState = HoverState::HoveringMidHigh;
            initialTargetAlpha = targetAlphaValue;
            midHighXoverParam.beginChangeGesture();
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        else { // Клик по области
            newDraggingState = DraggingState::None;
            initialHoverState = HoverState::None;
            initialTargetAlpha = 0.0f;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            // ... (onBandAreaClicked) ...
        }

        currentDraggingState = newDraggingState;
        currentHoverState = initialHoverState;
        // Запоминаем последнее состояние для цвета
        if (initialHoverState != HoverState::None) {
            lastHoverStateForColor = initialHoverState;
        }
        targetHighlightAlpha = initialTargetAlpha;
        // Мгновенно показываем при начале драга
        if (currentDraggingState != DraggingState::None) {
            currentHighlightAlpha = targetAlphaValue;
            repaint();
        }
        else if (currentHighlightAlpha > 0.0f && initialTargetAlpha == 0.0f) {
            // Если кликнули по области, а подсветка была, начать затухание
            repaint();
        }
    }

    // Перетаскивание мыши
    void AnalyzerOverlay::mouseDrag(const juce::MouseEvent& event)
    {
        if (currentDraggingState == DraggingState::None) return;

        // ... (обновление параметра newFreq) ...
        auto graphBounds = getGraphBounds();
        float mouseX = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), (float)event.x);
        float newFreq = xToFrequency(mouseX, graphBounds);
        if (currentDraggingState == DraggingState::DraggingLowMid) {
            newFreq = juce::jlimit(minLogFreq, midHighXoverParam.get() - 1.0f, newFreq);
            lowMidXoverParam.setValueNotifyingHost(lowMidXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            newFreq = juce::jlimit(lowMidXoverParam.get() + 1.0f, maxLogFreq, newFreq);
            midHighXoverParam.setValueNotifyingHost(midHighXoverParam.getNormalisableRange().convertTo0to1(newFreq));
        }


        // --- Управление видом (Курсор и Hover State для цвета) ---
        juce::MouseCursor cursor = juce::MouseCursor::LeftRightResizeCursor;
        HoverState hoverStateForUpdate = HoverState::None; // Состояние только для этого кадра

        float currentEventMouseX = (float)event.x;
        if (currentDraggingState == DraggingState::DraggingLowMid) {
            float currentLowMidX = mapFreqToXLog(lowMidXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            if (std::abs(currentEventMouseX - currentLowMidX) < dragTolerance) {
                hoverStateForUpdate = HoverState::HoveringLowMid;
            }
            else {
                cursor = juce::MouseCursor::NormalCursor;
            }
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            float currentMidHighX = mapFreqToXLog(midHighXoverParam.get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            if (std::abs(currentEventMouseX - currentMidHighX) < dragTolerance) {
                hoverStateForUpdate = HoverState::HoveringMidHigh;
            }
            else {
                cursor = juce::MouseCursor::NormalCursor;
            }
        }

        setMouseCursor(cursor);

        // Обновляем currentHoverState и lastHoverStateForColor
        // Если мышь ушла, currentHoverState станет None, но lastHoverStateForColor сохранится
        if (hoverStateForUpdate != currentHoverState) {
            currentHoverState = hoverStateForUpdate;
        }
        // Обновляем lastHoverStateForColor только если мышь НАД линией
        if (currentHoverState != HoverState::None) {
            lastHoverStateForColor = currentHoverState;
        }


        // --- Управление целевой альфой ---
        // Альфа остается максимальной В ТЕЧЕНИЕ ВСЕГО ДРАГА
        // Затухание начнется только в mouseUp -> mouseMove
        targetHighlightAlpha = targetAlphaValue; // Всегда видна во время драга
        // Но если альфа вдруг стала меньше (теоретически), подтягиваем ее
        if (currentHighlightAlpha < targetHighlightAlpha) {
            // Можно немного ускорить появление, если нужно
            // currentHighlightAlpha += (targetHighlightAlpha - currentHighlightAlpha) * alphaAnimationSpeed * 2.0f;
        }
    }

    // Отпускание мыши
    void AnalyzerOverlay::mouseUp(const juce::MouseEvent& event)
    {
        // ... (endChangeGesture) ...
        if (currentDraggingState == DraggingState::DraggingLowMid) {
            lowMidXoverParam.endChangeGesture();
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            midHighXoverParam.endChangeGesture();
        }


        // Запоминаем последнее состояние перед сбросом драга
        if (currentHoverState != HoverState::None) {
            lastHoverStateForColor = currentHoverState;
        }
        else if (currentDraggingState == DraggingState::DraggingLowMid) {
            lastHoverStateForColor = HoverState::HoveringLowMid; // Если ушли в сторону
        }
        else if (currentDraggingState == DraggingState::DraggingMidHigh) {
            lastHoverStateForColor = HoverState::HoveringMidHigh; // Если ушли в сторону
        }

        currentDraggingState = DraggingState::None; // Сбрасываем драг

        // Вызываем mouseMove, чтобы определить финальное состояние наведения и альфу
        mouseMove(event);
    }

} // namespace MBRP_GUI
// --- END OF FILE AnalyzerOverlay.cpp ---