#include "AnalyzerOverlay.h"
/*#include "../Source/PluginProcessor.h" */// Для доступа к параметрам, если потребуется, но лучше через конструктор
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace MBRP_GUI
{
    // Конструктор
    AnalyzerOverlay::AnalyzerOverlay(MBRPAudioProcessor& p) : // <--- ИЗМЕНЕНО
        processorRef(p) // <--- ИЗМЕНЕНО: сохраняем ссылку
    {
        setInterceptsMouseClicks(true, true);
        setPaintingIsUnclipped(true);
        startTimerHz(30);
    }

    // Отрисовка
    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        auto graphBounds = getGraphBounds();
        drawHoverHighlight(g, graphBounds);
        drawCrossoverLines(g, graphBounds);
        drawGainMarkers(g, graphBounds); // <--- НОВЫЙ ВЫЗОВ
    }

    // Таймер для анимации (пока без изменений, он управляет подсветкой кроссоверов)
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

        // Всегда перерисовываем, чтобы линии обновлялись, если параметр изменился извне
        // Это также обновит и маркеры Gain, если их параметры изменились
        needsRepaint = true;

        if (needsRepaint) {
            repaint();
        }
    }

    void AnalyzerOverlay::resized()
    {
        repaint();
    }

    juce::Rectangle<float> AnalyzerOverlay::getGraphBounds() const
    {
        // Убедитесь, что это совпадает с SpectrumAnalyzer!
        // Возможно, нужно будет немного уменьшить по высоте сверху, если маркеры Gain будут там
        return getLocalBounds().toFloat().reduced(1.f, 5.f);
    }

    float AnalyzerOverlay::xToFrequency(float x, const juce::Rectangle<float>& graphBounds) const
    {
        auto left = graphBounds.getX();
        auto width = graphBounds.getWidth();
        float freq = mapXToFreqLog(x, left, width, minLogFreq, maxLogFreq);
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

        float lmFreq = processorRef.lowMidCrossover->get(); // Используем processorRef
        float mFreq = processorRef.midCrossover->get();     // Используем processorRef
        float mhFreq = processorRef.midHighCrossover->get(); // Используем processorRef

        float lmX = mapFreqToXLog(lmFreq, left, width, minLogFreq, maxLogFreq);
        float mX = mapFreqToXLog(mFreq, left, width, minLogFreq, maxLogFreq);
        float mhX = mapFreqToXLog(mhFreq, left, width, minLogFreq, maxLogFreq);

        const auto lmCol = ColorScheme::getOrangeBorderColor();
        const auto mCol = ColorScheme::getMidBandColor();
        const auto mhCol = ColorScheme::getMidHighCrossoverColor();

        g.setColour(lmCol.withAlpha(0.7f));
        if (lmX >= left && lmX <= right) g.drawVerticalLine(roundToInt(lmX), top, bottom);

        g.setColour(mCol.withAlpha(0.7f));
        if (mX >= left && mX <= right) g.drawVerticalLine(roundToInt(mX), top, bottom);

        g.setColour(mhCol.withAlpha(0.7f));
        if (mhX >= left && mhX <= right) g.drawVerticalLine(roundToInt(mhX), top, bottom);

        const float handleSize = 8.0f;
        const float handleRadius = handleSize / 2.0f;

        auto drawHandle = [&](float xPos, Colour col) {
            if (xPos >= left && xPos <= right) {
                g.setColour(col);
                g.fillEllipse(xPos - handleRadius, top - handleRadius, handleSize, handleSize);
                g.setColour(col.darker(0.3f));
                g.drawEllipse(xPos - handleRadius, top - handleRadius, handleSize, handleSize, 1.0f);
            }
            };

        drawHandle(lmX, lmCol);
        drawHandle(mX, mCol);
        drawHandle(mhX, mhCol);
    }

    // --- НОВАЯ ФУНКЦИЯ для отрисовки маркеров Gain ---
    void AnalyzerOverlay::drawGainMarkers(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        using namespace juce;
        auto top = graphBounds.getY();
        auto height = graphBounds.getHeight();
        auto bottom = graphBounds.getBottom(); // <--- ДОБАВЬТЕ ЭТУ СТРОКУ
        auto leftGraphEdge = graphBounds.getX();
        auto rightGraphEdge = graphBounds.getRight();
        auto graphWidth = graphBounds.getWidth();

        // Получаем X-координаты кроссоверов
        float lmFreq = processorRef.lowMidCrossover->get();
        float mFreq = processorRef.midCrossover->get();
        float mhFreq = processorRef.midHighCrossover->get();

        float x0 = leftGraphEdge; // Начало Low полосы
        float x1 = mapFreqToXLog(lmFreq, leftGraphEdge, graphWidth, minLogFreq, maxLogFreq);
        float x2 = mapFreqToXLog(mFreq, leftGraphEdge, graphWidth, minLogFreq, maxLogFreq);
        float x3 = mapFreqToXLog(mhFreq, leftGraphEdge, graphWidth, minLogFreq, maxLogFreq);
        float x4 = rightGraphEdge; // Конец High полосы

        // Массивы для удобства
        float bandXStarts[] = { x0, x1, x2, x3 };
        float bandXEnds[] = { x1, x2, x3, x4 };
        std::atomic<float>* gainParams[] = {
            processorRef.lowGainParam,
            processorRef.lowMidGainParam,
            processorRef.midHighGainParam,
            processorRef.highGainParam
        };
        Colour bandColours[] = {
            ColorScheme::getLowBandColor(),
            ColorScheme::getLowMidBandColor(),
            ColorScheme::getMidHighBandColor(),
            ColorScheme::getHighBandAltColor()
        };

        const float markerHeight = 6.0f;
        const float markerHandleWidth = 8.0f;

        for (int i = 0; i < 4; ++i)
        {
            if (!gainParams[i]) continue;

            float currentGainDb = gainParams[i]->load();
            float yPos = mapGainDbToY(currentGainDb, top, height, gainMarkerMinDbOnGui, gainMarkerMaxDbOnGui);

            // Ограничиваем yPos границами графика
            // Здесь была ошибка с использованием 'bottom', если он не был объявлен в этой функции
            yPos = juce::jlimit(top + markerHeight / 2.0f, bottom - markerHeight / 2.0f, yPos); // Исправлено использование bottom

            float bandLeft = bandXStarts[i];
            float bandRight = bandXEnds[i];

            if (bandRight <= bandLeft || bandRight < leftGraphEdge || bandLeft > rightGraphEdge) continue;

            bandLeft = std::max(leftGraphEdge, bandLeft);
            bandRight = std::min(rightGraphEdge, bandRight);
            if (bandRight <= bandLeft) continue;

            g.setColour(bandColours[i]);
            juce::Rectangle<float> gainMarkerRect(bandLeft, yPos - markerHeight / 2.0f, bandRight - bandLeft, markerHeight);
            g.fillRect(gainMarkerRect);

            float centerX = bandLeft + (bandRight - bandLeft) / 2.0f;
            if (centerX >= leftGraphEdge && centerX <= rightGraphEdge) {
                g.setColour(bandColours[i].contrasting(0.7f));
                g.fillEllipse(centerX - markerHandleWidth / 2.0f, yPos - markerHandleWidth / 2.0f, markerHandleWidth, markerHandleWidth);
                g.setColour(bandColours[i].darker());
                g.drawEllipse(centerX - markerHandleWidth / 2.0f, yPos - markerHandleWidth / 2.0f, markerHandleWidth, markerHandleWidth, 1.0f);
            }
        }
    }

    void AnalyzerOverlay::drawHoverHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        // ... (Логика подсветки кроссоверов, пока без изменений) ...
        // Если будете добавлять подсветку для Gain, ее нужно будет интегрировать сюда или вызвать отдельно
        if (currentHighlightAlpha <= 0.0f) return;
        using namespace juce;
        float targetX = -1.0f;
        Colour highlightColour = Colours::transparentBlack;
        CrossoverHoverState stateToUse = CrossoverHoverState::None; // Используем состояния кроссовера
        if (currentCrossoverDragState != CrossoverDraggingState::None) {
            if (currentCrossoverDragState == CrossoverDraggingState::DraggingLowMid) stateToUse = CrossoverHoverState::HoveringLowMid;
            else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMid) stateToUse = CrossoverHoverState::HoveringMid;
            else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMidHigh) stateToUse = CrossoverHoverState::HoveringMidHigh;
        }
        else if (currentCrossoverHoverState != CrossoverHoverState::None) {
            stateToUse = currentCrossoverHoverState;
        }
        else {
            stateToUse = lastCrossoverHoverStateForColor;
        }
        if (stateToUse == CrossoverHoverState::None) return;
        auto left = graphBounds.getX(); auto width = graphBounds.getWidth();
        auto top = graphBounds.getY(); auto bottom = graphBounds.getBottom();
        if (stateToUse == CrossoverHoverState::HoveringLowMid) {
            targetX = mapFreqToXLog(processorRef.lowMidCrossover->get(), left, width, minLogFreq, maxLogFreq);
            highlightColour = ColorScheme::getOrangeBorderColor();
        }
        else if (stateToUse == CrossoverHoverState::HoveringMid) {
            targetX = mapFreqToXLog(processorRef.midCrossover->get(), left, width, minLogFreq, maxLogFreq);
            highlightColour = ColorScheme::getMidBandColor();
        }
        else {
            targetX = mapFreqToXLog(processorRef.midHighCrossover->get(), left, width, minLogFreq, maxLogFreq);
            highlightColour = ColorScheme::getMidHighCrossoverColor();
        }
        if (targetX >= left && targetX <= graphBounds.getRight()) {
            Rectangle<float> highlightRect;
            highlightRect.setWidth(highlightRectWidth);
            highlightRect.setCentre(targetX, graphBounds.getCentreY());
            highlightRect.setY(top);
            highlightRect.setBottom(bottom);
            g.setColour(highlightColour.withAlpha(currentHighlightAlpha));
            g.fillRect(highlightRect);
        }
    }

    std::pair<AnalyzerOverlay::GainDraggingState, juce::AudioParameterFloat*> AnalyzerOverlay::getGainInfoAt(const juce::MouseEvent& event, const juce::Rectangle<float>& graphBounds)
    {
        float mouseX = static_cast<float>(event.x);
        float mouseY = static_cast<float>(event.y);


        // Проверяем, что мышь внутри graphBounds (которые тоже в локальных координатах)
        if (!graphBounds.contains(mouseX, mouseY))
            return { GainDraggingState::None, nullptr };

        // Теперь все члены класса, такие как processorRef, minLogFreq, dragToleranceY, будут доступны
        float lmFreq = processorRef.lowMidCrossover->get();
        float mFreq = processorRef.midCrossover->get();
        float mhFreq = processorRef.midHighCrossover->get();

        float x0 = graphBounds.getX();
        float x1 = mapFreqToXLog(lmFreq, graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float x2 = mapFreqToXLog(mFreq, graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float x3 = mapFreqToXLog(mhFreq, graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
        float x4 = graphBounds.getRight();

        const juce::String gainParamIDs[] = { "lowGain", "lowMidGain", "midHighGain", "highGain" };
        float bandXStarts[] = { x0, x1, x2, x3 };
        float bandXEnds[] = { x1, x2, x3, x4 };
        // Используем полное имя типа для enum
        AnalyzerOverlay::GainDraggingState states[] = {
            AnalyzerOverlay::GainDraggingState::DraggingLowGain,
            AnalyzerOverlay::GainDraggingState::DraggingLowMidGain,
            AnalyzerOverlay::GainDraggingState::DraggingMidHighGain,
            AnalyzerOverlay::GainDraggingState::DraggingHighGain
        };

        for (int i = 0; i < 4; ++i)
        {
            if (mouseX >= bandXStarts[i] && mouseX < bandXEnds[i])
            {
                auto* paramBase = processorRef.getAPVTS().getParameter(gainParamIDs[i]);
                auto* param = dynamic_cast<juce::AudioParameterFloat*>(paramBase);

                if (param)
                {
                    float gainDb = param->get();
                    float markerY = mapGainDbToY(gainDb, graphBounds.getY(), graphBounds.getHeight(), gainMarkerMinDbOnGui, gainMarkerMaxDbOnGui);

                    if (std::abs(mouseY - markerY) < dragToleranceY)
                    {
                        return { states[i], param };
                    }
                }
            }
        }
        return { AnalyzerOverlay::GainDraggingState::None, nullptr };
    }


    void AnalyzerOverlay::mouseMove(const juce::MouseEvent& event)
    {
        if (currentCrossoverDragState != CrossoverDraggingState::None || currentGainDragState != GainDraggingState::None)
        {
            return;
        }

        auto graphBounds = getGraphBounds();
        CrossoverHoverState newCrossoverHover = CrossoverHoverState::None;
        auto gainInfo = getGainInfoAt(event, graphBounds);
        GainDraggingState hoveredGainState = gainInfo.first;

        float newTargetAlphaLocal = 0.0f;
        juce::MouseCursor cursor = juce::MouseCursor::NormalCursor;

        if (hoveredGainState != GainDraggingState::None) {
            cursor = juce::MouseCursor::UpDownResizeCursor;
            currentCrossoverHoverState = CrossoverHoverState::None;
            targetHighlightAlpha = 0.0f;
        }
        // Используем event.getLocalPosition() для contains
        else if (graphBounds.contains(event.getPosition().toFloat())) {
            float mouseX = static_cast<float>(event.x);
            float lmX = mapFreqToXLog(processorRef.lowMidCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mX = mapFreqToXLog(processorRef.midCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mhX = mapFreqToXLog(processorRef.midHighCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

            if (std::abs(mouseX - lmX) < dragToleranceX) newCrossoverHover = CrossoverHoverState::HoveringLowMid;
            else if (std::abs(mouseX - mX) < dragToleranceX) newCrossoverHover = CrossoverHoverState::HoveringMid;
            else if (std::abs(mouseX - mhX) < dragToleranceX) newCrossoverHover = CrossoverHoverState::HoveringMidHigh;

            if (newCrossoverHover != CrossoverHoverState::None) {
                newTargetAlphaLocal = targetAlphaValue;
                cursor = juce::MouseCursor::LeftRightResizeCursor;
            }
        }

        setMouseCursor(cursor);

        if (newCrossoverHover != CrossoverHoverState::None) {
            lastCrossoverHoverStateForColor = newCrossoverHover;
        }

        if (newCrossoverHover != currentCrossoverHoverState) {
            currentCrossoverHoverState = newCrossoverHover;
        }

        if (hoveredGainState == GainDraggingState::None && !juce::approximatelyEqual(newTargetAlphaLocal, targetHighlightAlpha)) {
            targetHighlightAlpha = newTargetAlphaLocal;
        }
        else if (hoveredGainState != GainDraggingState::None) {
            targetHighlightAlpha = 0.0f;
        }
    }

    void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    {
        auto graphBounds = getGraphBounds();
        juce::AudioParameterFloat* paramToChangeFromGain = nullptr;

        auto gainInfo = getGainInfoAt(event, graphBounds);
        currentGainDragState = gainInfo.first;
        paramToChangeFromGain = gainInfo.second;

        if (currentGainDragState != GainDraggingState::None && paramToChangeFromGain != nullptr)
        {
            paramToChangeFromGain->beginChangeGesture();
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            currentCrossoverHoverState = CrossoverHoverState::None;
            currentCrossoverDragState = CrossoverDraggingState::None;
            targetHighlightAlpha = 0.0f;
            // repaint(); // Не обязательно, таймер обновит
            return;
        }

        currentCrossoverDragState = CrossoverDraggingState::None;
        float mouseX = static_cast<float>(event.x);
        juce::AudioParameterFloat* paramToChangeFromXOver = nullptr;

        // Проверяем Y координату относительно graphBounds, а не всего компонента
        if (static_cast<float>(event.y) >= graphBounds.getY() && static_cast<float>(event.y) <= graphBounds.getBottom()) {
            // Доступ к параметрам кроссовера через processorRef
            float lmX = mapFreqToXLog(processorRef.lowMidCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mX = mapFreqToXLog(processorRef.midCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mhX = mapFreqToXLog(processorRef.midHighCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

            if (std::abs(mouseX - lmX) < dragToleranceX) {
                currentCrossoverDragState = CrossoverDraggingState::DraggingLowMid; paramToChangeFromXOver = processorRef.lowMidCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringLowMid;
            }
            else if (std::abs(mouseX - mX) < dragToleranceX) {
                currentCrossoverDragState = CrossoverDraggingState::DraggingMid; paramToChangeFromXOver = processorRef.midCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringMid;
            }
            else if (std::abs(mouseX - mhX) < dragToleranceX) {
                currentCrossoverDragState = CrossoverDraggingState::DraggingMidHigh; paramToChangeFromXOver = processorRef.midHighCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringMidHigh;
            }
        }

        if (currentCrossoverDragState != CrossoverDraggingState::None && paramToChangeFromXOver != nullptr) {
            lastCrossoverHoverStateForColor = currentCrossoverHoverState;
            targetHighlightAlpha = targetAlphaValue; currentHighlightAlpha = targetAlphaValue;
            paramToChangeFromXOver->beginChangeGesture();
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            // repaint();
        }
        else if (graphBounds.contains(event.getPosition().toFloat())) {
            currentCrossoverDragState = CrossoverDraggingState::None;
            currentGainDragState = GainDraggingState::None;
            if (currentCrossoverHoverState != CrossoverHoverState::None) { lastCrossoverHoverStateForColor = currentCrossoverHoverState; }
            currentCrossoverHoverState = CrossoverHoverState::None;
            targetHighlightAlpha = 0.0f;
            setMouseCursor(juce::MouseCursor::NormalCursor);

            float clickedFreq = xToFrequency(mouseX, graphBounds);
            int bandIndex = 0;
            // Доступ к параметрам кроссовера через processorRef
            if (clickedFreq < processorRef.lowMidCrossover->get()) bandIndex = 0;
            else if (clickedFreq < processorRef.midCrossover->get()) bandIndex = 1;
            else if (clickedFreq < processorRef.midHighCrossover->get()) bandIndex = 2;
            else bandIndex = 3;
            if (onBandAreaClicked) { onBandAreaClicked(bandIndex); }
        }
        else {
            currentCrossoverDragState = CrossoverDraggingState::None;
            currentGainDragState = GainDraggingState::None;
            targetHighlightAlpha = 0.0f; currentCrossoverHoverState = CrossoverHoverState::None;
        }
    }
    void AnalyzerOverlay::mouseExit(const juce::MouseEvent& /*event*/)
    {
        if (currentCrossoverDragState == CrossoverDraggingState::None && currentGainDragState == GainDraggingState::None) {
            if (currentCrossoverHoverState != CrossoverHoverState::None) {
                lastCrossoverHoverStateForColor = currentCrossoverHoverState;
            }
            targetHighlightAlpha = 0.0f;
            currentCrossoverHoverState = CrossoverHoverState::None;
            // currentGainHoverState = GainHoverState::None; // Если будет
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    //void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    //{
    //    auto graphBounds = getGraphBounds();
    //    juce::AudioParameterFloat* paramToChange = nullptr;

    //    // 1. Проверяем клик по Gain маркерам
    //    auto gainInfo = getGainInfoAt(event, graphBounds);
    //    currentGainDragState = gainInfo.first; // Устанавливаем состояние перетаскивания Gain
    //    paramToChange = gainInfo.second;       // Получаем параметр Gain

    //    if (currentGainDragState != GainDraggingState::None && paramToChange != nullptr)
    //    {
    //        paramToChange->beginChangeGesture();
    //        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    //        // Сбрасываем состояния и подсветку кроссовера
    //        currentCrossoverHoverState = CrossoverHoverState::None;
    //        currentCrossoverDragState = CrossoverDraggingState::None;
    //        targetHighlightAlpha = 0.0f;
    //        repaint(); // Может понадобиться для визуального отклика захвата Gain
    //        return;
    //    }

    //    // 2. Если не кликнули на Gain, проверяем кроссоверы
    //    currentCrossoverDragState = CrossoverDraggingState::None; // Сбрасываем на всякий случай
    //    float mouseX = static_cast<float>(event.x);
    //    paramToChange = nullptr; // Сбрасываем, т.к. он мог быть установлен от Gain

    //    if (static_cast<float>(event.y) >= graphBounds.getY() && static_cast<float>(event.y) <= graphBounds.getBottom()) { // Проверяем Y координату клика
    //        float lmX = mapFreqToXLog(processorRef.lowMidCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
    //        float mX = mapFreqToXLog(processorRef.midCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
    //        float mhX = mapFreqToXLog(processorRef.midHighCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

    //        if (std::abs(mouseX - lmX) < dragToleranceX) {
    //            currentCrossoverDragState = CrossoverDraggingState::DraggingLowMid; paramToChange = processorRef.lowMidCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringLowMid;
    //        }
    //        else if (std::abs(mouseX - mX) < dragToleranceX) {
    //            currentCrossoverDragState = CrossoverDraggingState::DraggingMid; paramToChange = processorRef.midCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringMid;
    //        }
    //        else if (std::abs(mouseX - mhX) < dragToleranceX) {
    //            currentCrossoverDragState = CrossoverDraggingState::DraggingMidHigh; paramToChange = processorRef.midHighCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringMidHigh;
    //        }
    //    }

    //    if (currentCrossoverDragState != CrossoverDraggingState::None && paramToChange != nullptr) {
    //        lastCrossoverHoverStateForColor = currentCrossoverHoverState;
    //        targetHighlightAlpha = targetAlphaValue; currentHighlightAlpha = targetAlphaValue;
    //        paramToChange->beginChangeGesture();
    //        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    //        repaint();
    //    }
    //    else if (graphBounds.contains(event.getPosition().toFloat())) {
    //        // Клик по пустой области
    //        currentCrossoverDragState = CrossoverDraggingState::None;
    //        currentGainDragState = GainDraggingState::None;
    //        if (currentCrossoverHoverState != CrossoverHoverState::None) { lastCrossoverHoverStateForColor = currentCrossoverHoverState; }
    //        currentCrossoverHoverState = CrossoverHoverState::None;
    //        targetHighlightAlpha = 0.0f;
    //        setMouseCursor(juce::MouseCursor::NormalCursor);

    //        float clickedFreq = xToFrequency(mouseX, graphBounds);
    //        int bandIndex = 0; // Определяем индекс полосы по клику
    //        if (clickedFreq < processorRef.lowMidCrossover->get()) bandIndex = 0;
    //        else if (clickedFreq < processorRef.midCrossover->get()) bandIndex = 1;
    //        else if (clickedFreq < processorRef.midHighCrossover->get()) bandIndex = 2;
    //        else bandIndex = 3;
    //        if (onBandAreaClicked) { onBandAreaClicked(bandIndex); }
    //        // if (currentHighlightAlpha > 0.0f) repaint(); // repaint будет вызван таймером
    //    }
    //    else {
    //        // Клик вне графика
    //        currentCrossoverDragState = CrossoverDraggingState::None;
    //        currentGainDragState = GainDraggingState::None;
    //        targetHighlightAlpha = 0.0f; currentCrossoverHoverState = CrossoverHoverState::None;
    //        // if (currentHighlightAlpha > 0.0f) repaint();
    //    }
    //}

    void AnalyzerOverlay::mouseDrag(const juce::MouseEvent& event)
    {
        auto graphBounds = getGraphBounds();

        if (currentGainDragState != GainDraggingState::None) {
            float mouseY = juce::jlimit(graphBounds.getY(), graphBounds.getBottom(), (float)event.y);
            float newGainDb = mapYToGainDb(mouseY, graphBounds.getY(), graphBounds.getHeight(), gainMarkerMinDbOnGui, gainMarkerMaxDbOnGui);

            juce::AudioParameterFloat* paramToUpdateGain = nullptr;
            const juce::String gainIDs[] = { "lowGain", "lowMidGain", "midHighGain", "highGain" };
            GainDraggingState gainStates[] = { GainDraggingState::DraggingLowGain, GainDraggingState::DraggingLowMidGain, GainDraggingState::DraggingMidHighGain, GainDraggingState::DraggingHighGain };

            for (int i = 0; i < 4; ++i) {
                if (currentGainDragState == gainStates[i]) {
                    paramToUpdateGain = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getAPVTS().getParameter(gainIDs[i]));
                    break;
                }
            }

            if (paramToUpdateGain) {
                newGainDb = juce::jlimit(paramToUpdateGain->getNormalisableRange().start, paramToUpdateGain->getNormalisableRange().end, newGainDb);
                paramToUpdateGain->setValueNotifyingHost(paramToUpdateGain->getNormalisableRange().convertTo0to1(newGainDb));
            }
            return;
        }

        if (currentCrossoverDragState != CrossoverDraggingState::None) {
            // ... (Логика для кроссоверов, как в предыдущем ответе, используя processorRef для доступа к параметрам)
            float mouseX = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), (float)event.x);
            float newFreqRaw = xToFrequency(mouseX, graphBounds);
            juce::AudioParameterFloat* paramToUpdateXOver = nullptr;
            float minAllowedFreq = this->minLogFreq; // Используем this-> для доступа к члену класса
            float maxAllowedFreq = this->maxLogFreq;

            if (currentCrossoverDragState == CrossoverDraggingState::DraggingLowMid) {
                paramToUpdateXOver = processorRef.lowMidCrossover;
                minAllowedFreq = juce::jmax(paramToUpdateXOver->getNormalisableRange().start, minLogFreq); // Исправлено minLogFreq
                maxAllowedFreq = juce::jmin(paramToUpdateXOver->getNormalisableRange().end, processorRef.midCrossover->get() - MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
            }
            else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMid) {
                paramToUpdateXOver = processorRef.midCrossover;
                minAllowedFreq = juce::jmax(paramToUpdateXOver->getNormalisableRange().start, processorRef.lowMidCrossover->get() + MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
                maxAllowedFreq = juce::jmin(paramToUpdateXOver->getNormalisableRange().end, processorRef.midHighCrossover->get() - MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
            }
            else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMidHigh) {
                paramToUpdateXOver = processorRef.midHighCrossover;
                minAllowedFreq = juce::jmax(paramToUpdateXOver->getNormalisableRange().start, processorRef.midCrossover->get() + MBRPAudioProcessor::MIN_CROSSOVER_SEPARATION);
                maxAllowedFreq = juce::jmin(maxAllowedFreq, paramToUpdateXOver->getNormalisableRange().end); // Исправлено maxLogFreq
            }

            if (paramToUpdateXOver) {
                float finalFreq = juce::jlimit(minAllowedFreq, maxAllowedFreq, newFreqRaw);
                paramToUpdateXOver->setValueNotifyingHost(paramToUpdateXOver->getNormalisableRange().convertTo0to1(finalFreq));
            }

            // Обновление состояния наведения для подсветки кроссовера
            CrossoverHoverState hoverStateForUpdate = CrossoverHoverState::None;
            // ... (логика определения hoverStateForUpdate для кроссовера, как была раньше)
            if (currentCrossoverDragState == CrossoverDraggingState::DraggingLowMid && std::abs(mouseX - mapFreqToXLog(processorRef.lowMidCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq)) < dragToleranceX * 2.f) hoverStateForUpdate = CrossoverHoverState::HoveringLowMid;
            // ... и для других кроссоверов ...
            if (hoverStateForUpdate != currentCrossoverHoverState) currentCrossoverHoverState = hoverStateForUpdate;
            if (currentCrossoverHoverState != CrossoverHoverState::None) lastCrossoverHoverStateForColor = currentCrossoverHoverState;
            targetHighlightAlpha = targetAlphaValue;
            return;
        }
    }

    void AnalyzerOverlay::mouseUp(const juce::MouseEvent& event)
    {
        if (currentGainDragState != GainDraggingState::None) {
            juce::AudioParameterFloat* paramToEndGestureGain = nullptr;
            const juce::String gainIDs[] = { "lowGain", "lowMidGain", "midHighGain", "highGain" };
            GainDraggingState gainStates[] = { GainDraggingState::DraggingLowGain, GainDraggingState::DraggingLowMidGain, GainDraggingState::DraggingMidHighGain, GainDraggingState::DraggingHighGain };
            for (int i = 0; i < 4; ++i) {
                if (currentGainDragState == gainStates[i]) {
                    paramToEndGestureGain = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getAPVTS().getParameter(gainIDs[i]));
                    break;
                }
            }
            if (paramToEndGestureGain) paramToEndGestureGain->endChangeGesture();
            currentGainDragState = GainDraggingState::None;
            mouseMove(event);
            return;
        }

        if (currentCrossoverDragState != CrossoverDraggingState::None) {
            juce::AudioParameterFloat* paramToEndGestureXOver = nullptr;
            if (currentCrossoverDragState == CrossoverDraggingState::DraggingLowMid) paramToEndGestureXOver = processorRef.lowMidCrossover;
            else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMid) paramToEndGestureXOver = processorRef.midCrossover;
            else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMidHigh) paramToEndGestureXOver = processorRef.midHighCrossover;
            if (paramToEndGestureXOver) { paramToEndGestureXOver->endChangeGesture(); }

            // Обновляем lastCrossoverHoverStateForColor корректно
            CrossoverHoverState hoverAtMouseUp = CrossoverHoverState::None;
            auto graphBounds = getGraphBounds(); // Получаем graphBounds снова
            if (graphBounds.contains(event.getPosition().toFloat())) { // Используем getLocalPosition
                float mouseX = static_cast<float>(event.x);
                float lmX = mapFreqToXLog(processorRef.lowMidCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
                float mX = mapFreqToXLog(processorRef.midCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
                float mhX = mapFreqToXLog(processorRef.midHighCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
                if (std::abs(mouseX - lmX) < dragToleranceX) hoverAtMouseUp = CrossoverHoverState::HoveringLowMid;
                else if (std::abs(mouseX - mX) < dragToleranceX) hoverAtMouseUp = CrossoverHoverState::HoveringMid;
                else if (std::abs(mouseX - mhX) < dragToleranceX) hoverAtMouseUp = CrossoverHoverState::HoveringMidHigh;
            }
            if (hoverAtMouseUp != CrossoverHoverState::None) {
                lastCrossoverHoverStateForColor = hoverAtMouseUp;
            }
            else {
                // Если мышь не над линией, но перетаскивание было, сохраняем последнее состояние перетаскивания
                if (currentCrossoverDragState == CrossoverDraggingState::DraggingLowMid) lastCrossoverHoverStateForColor = CrossoverHoverState::HoveringLowMid;
                else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMid) lastCrossoverHoverStateForColor = CrossoverHoverState::HoveringMid;
                else if (currentCrossoverDragState == CrossoverDraggingState::DraggingMidHigh) lastCrossoverHoverStateForColor = CrossoverHoverState::HoveringMidHigh;
            }
            currentCrossoverDragState = CrossoverDraggingState::None;
            mouseMove(event);
            return;
        }
    }

}
