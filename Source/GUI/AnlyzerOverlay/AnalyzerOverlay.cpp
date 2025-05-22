#include "AnalyzerOverlay.h"
/*#include "../Source/PluginProcessor.h" */// Для доступа к параметрам, если потребуется, но лучше через конструктор
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace MBRP_GUI
{
    // Конструктор
    AnalyzerOverlay::AnalyzerOverlay(MBRPAudioProcessor& p) : // <--- ИЗМЕНЕНО
        processorRef(p), // <--- ИЗМЕНЕНО: сохраняем ссылку
        popupHideDelayFramesCounter(0)
    {
        setInterceptsMouseClicks(true, true);
        setPaintingIsUnclipped(true);
        startTimerHz(30);

        const juce::String soloParamIDs[] = { "lowSolo", "lowMidSolo", "midHighSolo", "highSolo" };
        const juce::String muteParamIDs[] = { "lowMute", "lowMidMute", "midHighMute", "highMute" };
        const juce::String bypassParamIDs[] = { "lowBypass", "lowMidBypass", "midHighBypass", "highBypass" }; // <--- НОВОЕ

        for (int i = 0; i < numBands; ++i)
        {
            soloButtons[i].setButtonText("S");
            soloButtons[i].setClickingTogglesState(true);
            addAndMakeVisible(soloButtons[i]);
            soloAttachments[i] = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), soloParamIDs[i], soloButtons[i]);
            soloButtons[i].setTooltip("Solo Band " + juce::String(i + 1)); // Или более осмысленные имена полос

            muteButtons[i].setButtonText("M");
            muteButtons[i].setClickingTogglesState(true);
            addAndMakeVisible(muteButtons[i]);
            muteAttachments[i] = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), muteParamIDs[i], muteButtons[i]);
            muteButtons[i].setTooltip("Mute Band " + juce::String(i + 1));

            // --- НОВОЕ: Bypass кнопки для каждой полосы ---
            bypassButtons[i].setButtonText("B");
            bypassButtons[i].setClickingTogglesState(true);
            addAndMakeVisible(bypassButtons[i]);
            bypassAttachments[i] = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), bypassParamIDs[i], bypassButtons[i]);
            bypassButtons[i].setTooltip("Bypass Band " + juce::String(i + 1) + " Effects");
            // ---------------------------------------------

            // Все кнопки видимы всегда
            soloButtons[i].setVisible(true);
            muteButtons[i].setVisible(true);
            bypassButtons[i].setVisible(true); // <--- НОВОЕ
        }

        gainPopupDisplay.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey.withAlpha(0.85f));
        gainPopupDisplay.setColour(juce::Label::textColourId, juce::Colours::white);
        gainPopupDisplay.setColour(juce::Label::outlineColourId, juce::Colours::lightgrey);
        gainPopupDisplay.setBorderSize({ 1, 1, 1, 1 });
        gainPopupDisplay.setJustificationType(juce::Justification::centred);
        gainPopupDisplay.setFont(14.0f);
        addChildComponent(gainPopupDisplay);
        gainPopupDisplay.setVisible(false);

    }

    AnalyzerOverlay::~AnalyzerOverlay()
    {
        for (int i = 0; i < numBands; ++i) {
            soloAttachments[i].reset();
            muteAttachments[i].reset();
            bypassAttachments[i].reset(); // <--- НОВОЕ
        }
    }

    void AnalyzerOverlay::setActiveBand(int bandIndex)
    {
        if (bandIndex >= 0 && bandIndex < numBands)
        {
            activeBandIndex = bandIndex;
            repaint(); // Перерисовать, чтобы обновить подсветку
        }
    }


    // Отрисовка
    void AnalyzerOverlay::paint(juce::Graphics& g)
    {
        auto graphBounds = getGraphBounds();
        // Рисуем подсветку активной полосы Gain ДО линий и маркеров
        drawGainMarkersAndActiveBandHighlight(g, graphBounds);
        drawHoverHighlight(g, graphBounds); // Подсветка для перетаскивания кроссоверов
        drawCrossoverLines(g, graphBounds);
        // drawGainMarkers(g, graphBounds); // Теперь это часть drawGainMarkersAndActiveBandHighlight
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

        if (popupHideDelayFramesCounter > 0)
        {
            popupHideDelayFramesCounter--;
            if (popupHideDelayFramesCounter == 0)
            {
                hideGainPopup(); // Скрываем pop-up по истечении задержки
                // needsRepaint уже будет true, если pop-up был виден
            }
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
        positionBandControls(getGraphBounds());
    }

    void AnalyzerOverlay::positionBandControls(const juce::Rectangle<float>& graphBounds)
    {
        auto leftGraphEdge = graphBounds.getX();
        auto graphWidth = graphBounds.getWidth();

        // Позиция кнопок (например, вверху каждой полосы)
        float buttonY = graphBounds.getY() + 3.0f; // Небольшой отступ сверху
        int buttonWidth = 18;  // Маленький размер для кнопок
        int buttonHeight = 16; //
        int buttonSpacing = 2;  // Расстояние между кнопками S, M, B

        float lmFreq = processorRef.lowMidCrossover->get();
        float mFreq = processorRef.midCrossover->get();
        float mhFreq = processorRef.midHighCrossover->get();

        float x_coords[] = {
            leftGraphEdge,
            mapFreqToXLog(lmFreq, leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            mapFreqToXLog(mFreq,  leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            mapFreqToXLog(mhFreq, leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            graphBounds.getRight()
        };

        for (int i = 0; i < numBands; ++i)
        {
            float bandLeftX = x_coords[i];
            float bandRightX = x_coords[i + 1];

            bool canShowButtons = true;
            if (bandRightX <= bandLeftX || bandRightX < leftGraphEdge || bandLeftX > graphBounds.getRight()) {
                canShowButtons = false;
            }
            bandLeftX = std::max(leftGraphEdge, bandLeftX);
            bandRightX = std::min(graphBounds.getRight(), bandRightX);
            if (bandRightX <= bandLeftX) {
                canShowButtons = false;
            }

            float bandWidthCurrent = bandRightX - bandLeftX;
            float totalButtonsWidth = buttonWidth * 3 + buttonSpacing * 2;

            if (bandWidthCurrent < totalButtonsWidth + 4) { // + небольшой запас
                canShowButtons = false; // Слишком узкая полоса для кнопок
            }

            if (canShowButtons) {
                // Позиционируем кнопки S, M, B рядом друг с другом
                // Например, центрируем группу кнопок внутри полосы
                float buttonsGroupStartX = bandLeftX + (bandWidthCurrent - totalButtonsWidth) / 2.0f;

                bypassButtons[i].setBounds(juce::roundToInt(buttonsGroupStartX), juce::roundToInt(buttonY), buttonWidth, buttonHeight);
                soloButtons[i].setBounds(juce::roundToInt(buttonsGroupStartX + buttonWidth + buttonSpacing), juce::roundToInt(buttonY), buttonWidth, buttonHeight);
                muteButtons[i].setBounds(juce::roundToInt(buttonsGroupStartX + (buttonWidth + buttonSpacing) * 2), juce::roundToInt(buttonY), buttonWidth, buttonHeight);

                bypassButtons[i].setVisible(true);
                soloButtons[i].setVisible(true);
                muteButtons[i].setVisible(true);
            }
            else {
                bypassButtons[i].setVisible(false);
                soloButtons[i].setVisible(false);
                muteButtons[i].setVisible(false);
            }
        }
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

    // Переименованная функция, теперь включает подсветку активной полосы
    void AnalyzerOverlay::drawGainMarkersAndActiveBandHighlight(juce::Graphics& g, juce::Rectangle<float> graphBounds)
    {
        using namespace juce;
        auto top = graphBounds.getY();
        auto height = graphBounds.getHeight();
        auto bottom = graphBounds.getBottom();
        auto leftGraphEdge = graphBounds.getX();
        auto rightGraphEdge = graphBounds.getRight();
        auto graphWidth = graphBounds.getWidth();

        float lmFreq = processorRef.lowMidCrossover->get();
        float mFreq = processorRef.midCrossover->get();
        float mhFreq = processorRef.midHighCrossover->get();

        float x_coords[] = {
            leftGraphEdge,
            mapFreqToXLog(lmFreq, leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            mapFreqToXLog(mFreq,  leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            mapFreqToXLog(mhFreq, leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            rightGraphEdge
        };

        const juce::String gainParamIDs[] = { "lowGain", "lowMidGain", "midHighGain", "highGain" };
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
            auto* paramBase = processorRef.getAPVTS().getParameter(gainParamIDs[i]);
            auto* gainParam = dynamic_cast<juce::AudioParameterFloat*>(paramBase);
            if (!gainParam) continue;

            float currentGainDb = gainParam->get();
            float yPos = mapGainDbToY(currentGainDb, top, height, gainMarkerMinDbOnGui, gainMarkerMaxDbOnGui);
            yPos = juce::jlimit(top + markerHeight / 2.0f, bottom - markerHeight / 2.0f, yPos);

            float bandLeftX = x_coords[i];
            float bandRightX = x_coords[i + 1];

            // Пропускаем отрисовку, если полоса невидима или нулевой ширины
            if (bandRightX <= bandLeftX || bandRightX < leftGraphEdge || bandLeftX > rightGraphEdge) continue;
            bandLeftX = std::max(leftGraphEdge, bandLeftX);
            bandRightX = std::min(rightGraphEdge, bandRightX);
            if (bandRightX <= bandLeftX) continue;

            // --- НОВОЕ: Отрисовка градиентной подсветки для АКТИВНОЙ полосы ---
            if (i == activeBandIndex)
            {
                juce::Colour baseColour = bandColours[i];
                // Градиент от цвета полосы (внизу) к почти прозрачному (вверху)
                juce::ColourGradient gradient(baseColour.withAlpha(0.3f), // Цвет у маркера Gain
                    bandLeftX, yPos,
                    baseColour.withAlpha(0.0f), // Прозрачный цвет у верхней границы графика
                    bandLeftX, top,
                    false);                     // не радиальный

                // Можно сделать градиент на всю высоту полосы, если маркер по центру
                // juce::ColourGradient gradient (baseColour.withAlpha(0.0f), bandLeftX, top,
                //                                baseColour.withAlpha(0.4f), bandLeftX, bottom, false);
                // gradient.addColour(0.5, baseColour.withAlpha(0.1f)); // для нелинейности

                g.setGradientFill(gradient);
                g.fillRect(bandLeftX, top, bandRightX - bandLeftX, height);
            }
            // --------------------------------------------------------------

            // Отрисовка самого маркера Gain
            g.setColour(bandColours[i]);
            juce::Rectangle<float> gainMarkerRect(bandLeftX, yPos - markerHeight / 2.0f, bandRightX - bandLeftX, markerHeight);
            g.fillRect(gainMarkerRect);

            float centerX = bandLeftX + (bandRightX - bandLeftX) / 2.0f;
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

    void AnalyzerOverlay::showGainPopup(const juce::MouseEvent* eventForPosition, float valueDb)
    {
        popupHideDelayFramesCounter = 0; // Останавливаем задержку на скрытие, если показываем снова

        juce::String text = juce::String(valueDb, 1) + " dB";
        gainPopupDisplay.setText(text, juce::dontSendNotification);

        int textWidth = gainPopupDisplay.getFont().getStringWidth(text);
        int popupWidth = textWidth + 20;
        int popupHeight = 25;

        // Позиционирование
        if (eventForPosition) // Если передали MouseEvent, позиционируем относительно мыши
        {
            auto mousePos = eventForPosition->getPosition();
            int x = mousePos.x + 15;
            int y = mousePos.y + 15;

            if (getParentComponent()) { /* ... логика ограничения родительским компонентом ... */ }
            gainPopupDisplay.setBounds(x, y, popupWidth, popupHeight);
        }
        // Если eventForPosition == nullptr, pop-up обновит текст, но не позицию
        // (можно добавить логику для позиционирования по центру активного маркера, если нужно)

        if (!gainPopupDisplay.isVisible()) {
            gainPopupDisplay.setVisible(true);
            // gainPopupDisplay.toFront(true); // Если нужно
        }
    }

    void AnalyzerOverlay::hideGainPopup()
    {
        if (gainPopupDisplay.isVisible()) {
            gainPopupDisplay.setVisible(false);
        }
        // Сбрасываем состояния, связанные с pop-up, только если он действительно скрывается
        // и не удерживается из-за перетаскивания.
        // Это лучше делать в mouseUp или когда курсор уходит от маркера.
        // currentlyHoveredOrDraggedGainState = GainDraggingState::None;
        // currentlyHoveredOrDraggedGainParam = nullptr;
    }

    void AnalyzerOverlay::startPopupHideDelay()
    {
        if (gainPopupDisplay.isVisible() && currentGainDragState == GainDraggingState::None) { // Скрываем только если не перетаскиваем
            popupHideDelayFramesCounter = POPUP_HIDE_DELAY_TOTAL_FRAMES;
        }
    }



    void AnalyzerOverlay::mouseDoubleClick(const juce::MouseEvent& event)
    {
        auto graphBounds = getGraphBounds();

        // Проверяем, был ли двойной клик по одному из маркеров Gain
        auto gainInfo = getGainInfoAt(event, graphBounds);
        juce::AudioParameterFloat* paramToReset = gainInfo.second;

        if (paramToReset != nullptr) // Если да, сбрасываем параметр
        {
            // Значение, к которому хотим сбросить (0.0 dB для Gain)
            float targetValueDB = 0.0f;

            // Получаем диапазон нормализации параметра
            auto normalizedRange = paramToReset->getNormalisableRange();

            // Преобразуем целевое значение dB в нормализованное значение (0.0 to 1.0)
            // Убедимся, что targetValueDB находится в пределах диапазона параметра,
            // хотя для 0.0dB это обычно так.
            float normalizedValue = normalizedRange.convertTo0to1(juce::jlimit(normalizedRange.start, normalizedRange.end, targetValueDB));

            // Начинаем изменение жестом, устанавливаем значение и завершаем жест
            paramToReset->beginChangeGesture();
            paramToReset->setValueNotifyingHost(normalizedValue);
            paramToReset->endChangeGesture();

            // repaint(); // Немедленная перерисовка, чтобы увидеть сброшенное значение
                         // Хотя таймер тоже должен это сделать

            showGainPopup(&event, targetValueDB); // Показываем сброшенное значение
            startPopupHideDelay();
            return; // Выходим, так как действие выполнено
        }
    }

    void AnalyzerOverlay::mouseMove(const juce::MouseEvent& event)
    {
        if (currentCrossoverDragState != CrossoverDraggingState::None) {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            startPopupHideDelay();
            return;
        }

        if (currentGainDragState != GainDraggingState::None) {
            // Во время перетаскивания Gain, pop-up обновляется в mouseDrag
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            return;
        }

        auto graphBounds = getGraphBounds();
        auto gainInfo = getGainInfoAt(event, graphBounds); // Использует event.x, event.y
        GainDraggingState hoveredGainStateNow = gainInfo.first;
        juce::AudioParameterFloat* hoveredGainParamNow = gainInfo.second;

        CrossoverHoverState newCrossoverHover = CrossoverHoverState::None;
        float newTargetAlphaLocal = 0.0f;
        juce::MouseCursor cursor = juce::MouseCursor::NormalCursor;

        if (hoveredGainStateNow != GainDraggingState::None && hoveredGainParamNow != nullptr)
        {
            currentlyHoveredOrDraggedGainState = hoveredGainStateNow; // Запоминаем для pop-up
            currentlyHoveredOrDraggedGainParam = hoveredGainParamNow;
            showGainPopup(&event, hoveredGainParamNow->get());
            cursor = juce::MouseCursor::UpDownResizeCursor;
            targetHighlightAlpha = 0.0f;
            currentCrossoverHoverState = CrossoverHoverState::None;
        }
        else // Не наведены на Gain, проверяем кроссоверы
        {
            if (currentlyHoveredOrDraggedGainState != GainDraggingState::None) {
                startPopupHideDelay();
            }
            currentlyHoveredOrDraggedGainState = GainDraggingState::None;
            currentlyHoveredOrDraggedGainParam = nullptr;

            if (graphBounds.contains(event.getPosition().toFloat())) {
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
        }

        setMouseCursor(cursor);
        if (newCrossoverHover != CrossoverHoverState::None) {
            lastCrossoverHoverStateForColor = newCrossoverHover;
        }
        if (newCrossoverHover != currentCrossoverHoverState) {
            currentCrossoverHoverState = newCrossoverHover;
        }
        if (!juce::approximatelyEqual(newTargetAlphaLocal, targetHighlightAlpha)) { // Обновляем альфу только если нужно
            targetHighlightAlpha = newTargetAlphaLocal;
        }
    }

    void AnalyzerOverlay::mouseDown(const juce::MouseEvent& event)
    {

        for (int i = 0; i < numBands; ++i) {
            if (event.eventComponent == &soloButtons[i] ||
                event.eventComponent == &muteButtons[i] ||
                event.eventComponent == &bypassButtons[i])
            {
                // Клик был по одной из кнопок B/S/M.
                // Ничего не делаем здесь, позволяем кнопке обработать событие.
                // Важно, чтобы эти кнопки были добавлены как дочерние к AnalyzerOverlay.
                return;
            }
        }
        popupHideDelayFramesCounter = 0;

        auto graphBounds = getGraphBounds();
        auto gainInfo = getGainInfoAt(event, graphBounds); // Использует event.x, event.y
        currentGainDragState = gainInfo.first;
        juce::AudioParameterFloat* paramToChange = gainInfo.second;

        if (currentGainDragState != GainDraggingState::None && paramToChange != nullptr)
        {
            currentlyHoveredOrDraggedGainParam = paramToChange;
            currentlyHoveredOrDraggedGainState = currentGainDragState;
            showGainPopup(&event, paramToChange->get());

            paramToChange->beginChangeGesture();
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            currentCrossoverHoverState = CrossoverHoverState::None;
            //currentCrossoverDragState = CrossoverDraggingState::None;
            targetHighlightAlpha = 0.0f;

            int bandIndex = -1;
            if (currentGainDragState == GainDraggingState::DraggingLowGain) bandIndex = 0;
            else if (currentGainDragState == GainDraggingState::DraggingLowMidGain) bandIndex = 1;
            else if (currentGainDragState == GainDraggingState::DraggingMidHighGain) bandIndex = 2;
            else if (currentGainDragState == GainDraggingState::DraggingHighGain) bandIndex = 3;
            if (bandIndex != -1) {
                setActiveBand(bandIndex);
                if (onBandAreaClicked) {
                    onBandAreaClicked(bandIndex);
                }
            }
            return;
        }

        hideGainPopup();
        currentCrossoverDragState = CrossoverDraggingState::None;
        float mouseX = static_cast<float>(event.x);
        paramToChange = nullptr;

        if (static_cast<float>(event.y) >= graphBounds.getY() && static_cast<float>(event.y) <= graphBounds.getBottom()) {
            // ... (логика определения currentCrossoverDragState и paramToChange для кроссовера)
            float lmX = mapFreqToXLog(processorRef.lowMidCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mX = mapFreqToXLog(processorRef.midCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            float mhX = mapFreqToXLog(processorRef.midHighCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
            if (std::abs(mouseX - lmX) < dragToleranceX) {
                currentCrossoverDragState = CrossoverDraggingState::DraggingLowMid; paramToChange = processorRef.lowMidCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringLowMid;
            }
            else if (std::abs(mouseX - mX) < dragToleranceX) {
                currentCrossoverDragState = CrossoverDraggingState::DraggingMid; paramToChange = processorRef.midCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringMid;
            }
            else if (std::abs(mouseX - mhX) < dragToleranceX) {
                currentCrossoverDragState = CrossoverDraggingState::DraggingMidHigh; paramToChange = processorRef.midHighCrossover; currentCrossoverHoverState = CrossoverHoverState::HoveringMidHigh;
            }
        }

        if (currentCrossoverDragState != CrossoverDraggingState::None && paramToChange != nullptr) {
            lastCrossoverHoverStateForColor = currentCrossoverHoverState;
            targetHighlightAlpha = targetAlphaValue; currentHighlightAlpha = targetAlphaValue;
            paramToChange->beginChangeGesture();
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        else if (graphBounds.contains(event.getPosition().toFloat())) {
            currentCrossoverDragState = CrossoverDraggingState::None;
            currentGainDragState = GainDraggingState::None; // Убедимся, что сброшено
            currentlyHoveredOrDraggedGainState = GainDraggingState::None; // И это тоже
            currentlyHoveredOrDraggedGainParam = nullptr;

            if (currentCrossoverHoverState != CrossoverHoverState::None) { lastCrossoverHoverStateForColor = currentCrossoverHoverState; }
            currentCrossoverHoverState = CrossoverHoverState::None;
            targetHighlightAlpha = 0.0f;
            setMouseCursor(juce::MouseCursor::NormalCursor);

            float clickedFreq = xToFrequency(mouseX, graphBounds);
            int bandIndexClicked = 0;
            if (clickedFreq < processorRef.lowMidCrossover->get()) bandIndexClicked = 0;
            else if (clickedFreq < processorRef.midCrossover->get()) bandIndexClicked = 1;
            else if (clickedFreq < processorRef.midHighCrossover->get()) bandIndexClicked = 2;
            else bandIndexClicked = 3;
            if (onBandAreaClicked) { onBandAreaClicked(bandIndexClicked); }
        }
        else {
            currentCrossoverDragState = CrossoverDraggingState::None;
            currentGainDragState = GainDraggingState::None;
            currentlyHoveredOrDraggedGainState = GainDraggingState::None;
            currentlyHoveredOrDraggedGainParam = nullptr;
            targetHighlightAlpha = 0.0f; currentCrossoverHoverState = CrossoverHoverState::None;
        }
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
            // Убедимся, что currentlyHoveredOrDraggedGainParam все еще валиден (хотя должен быть после mouseDown)
            if (!currentlyHoveredOrDraggedGainParam) {
                currentGainDragState = GainDraggingState::None; // Безопасный выход
                return;
            }

            float mouseY = juce::jlimit(graphBounds.getY(), graphBounds.getBottom(), static_cast<float>(event.y));
            float newGainDb = mapYToGainDb(mouseY, graphBounds.getY(), graphBounds.getHeight(), gainMarkerMinDbOnGui, gainMarkerMaxDbOnGui);

            newGainDb = juce::jlimit(currentlyHoveredOrDraggedGainParam->getNormalisableRange().start,
                currentlyHoveredOrDraggedGainParam->getNormalisableRange().end,
                newGainDb);
            currentlyHoveredOrDraggedGainParam->setValueNotifyingHost(currentlyHoveredOrDraggedGainParam->getNormalisableRange().convertTo0to1(newGainDb));

            showGainPopup(&event, newGainDb); // Обновляем pop-up в реальном времени
            popupHideDelayFramesCounter = 0; // Не даем pop-up скрыться во время драга
            return;
        }

        if (currentCrossoverDragState != CrossoverDraggingState::None) {
            startPopupHideDelay();
            float mouseX = juce::jlimit(graphBounds.getX(), graphBounds.getRight(), static_cast<float>(event.x));
            // ... (остальная логика для кроссовер drag, включая проверку paramToUpdateXOver != nullptr перед использованием) ...
            float newFreqRaw = xToFrequency(mouseX, graphBounds);
            juce::AudioParameterFloat* paramToUpdateXOver = nullptr;
            float minAllowedFreq = this->minLogFreq;
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

            if (paramToUpdateXOver) { // <--- ДОБАВИТЬ ПРОВЕРКУ
                float finalFreq = juce::jlimit(minAllowedFreq, maxAllowedFreq, newFreqRaw);
                paramToUpdateXOver->setValueNotifyingHost(paramToUpdateXOver->getNormalisableRange().convertTo0to1(finalFreq));
            }
            targetHighlightAlpha = targetAlphaValue;
            return;
        }
    }

    void AnalyzerOverlay::mouseUp(const juce::MouseEvent& event)
    {
        if (currentGainDragState != GainDraggingState::None) {
            if (currentlyHoveredOrDraggedGainParam) { // Проверка на nullptr
                currentlyHoveredOrDraggedGainParam->endChangeGesture();
            }
            // currentlyHoveredOrDraggedGainState и currentlyHoveredOrDraggedGainParam НЕ сбрасываем здесь,
            // чтобы mouseMove мог решить, нужно ли запускать таймер скрытия, если мышь осталась над маркером.
            startPopupHideDelay();
            currentGainDragState = GainDraggingState::None; // Сбрасываем только состояние перетаскивания
            mouseMove(event);
            return;
        }

        if (currentCrossoverDragState != CrossoverDraggingState::None) {
            startPopupHideDelay();
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

    void AnalyzerOverlay::mouseExit(const juce::MouseEvent& /*event*/) // event можно пометить juce::ignoreUnused, если он не используется внутри
    {
        // Эта функция вызывается, когда курсор мыши покидает границы компонента AnalyzerOverlay.

        // Скрываем любые активные элементы интерфейса (подсветку кроссоверов, pop-up для Gain)
        // только в том случае, если пользователь не находится в процессе перетаскивания
        // какого-либо элемента (кроссовера или маркера Gain). Если он перетаскивает,
        // то mouseUp обработает завершение действия, даже если мышь вышла за пределы.
        if (currentCrossoverDragState == CrossoverDraggingState::None && currentGainDragState == GainDraggingState::None)
        {
            // 1. Обработка подсветки кроссовера:
            // Если была активная подсветка при наведении, запоминаем ее цвет для плавного затухания.
            if (currentCrossoverHoverState != CrossoverHoverState::None) {
                lastCrossoverHoverStateForColor = currentCrossoverHoverState;
            }
            targetHighlightAlpha = 0.0f; // Запускаем анимацию затухания подсветки кроссовера.
            currentCrossoverHoverState = CrossoverHoverState::None; // Сбрасываем состояние наведения на кроссовер.

            // 2. Обработка pop-up дисплея для Gain:
            // Если pop-up был виден или мы отслеживали наведение на маркер Gain,
            // запускаем таймер для его отложенного скрытия.
            if (gainPopupDisplay.isVisible() || currentlyHoveredOrDraggedGainState != GainDraggingState::None)
            {
                startPopupHideDelay(); // Этот метод установит popupHideDelayFramesCounter.
            }
            // Сбрасываем состояния, связанные с наведением/перетаскиванием Gain,
            // так как мышь покинула компонент, и мы не перетаскиваем.
            currentlyHoveredOrDraggedGainState = GainDraggingState::None;
            currentlyHoveredOrDraggedGainParam = nullptr;
        }

        // В любом случае, когда мышь покидает компонент, возвращаем стандартный курсор,
        // если только не идет активное перетаскивание (в этом случае курсор останется таким,
        // каким его установил mouseDown/mouseDrag).
        if (currentCrossoverDragState == CrossoverDraggingState::None && currentGainDragState == GainDraggingState::None)
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
    }


}
