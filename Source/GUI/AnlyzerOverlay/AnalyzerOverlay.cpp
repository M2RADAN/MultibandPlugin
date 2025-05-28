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
        popupHideDelayFramesCounter(0),
        currentlyHoveredCrossoverState(CrossoverHoverState::None),
        currentlyHoveredOrDraggedCrossoverParam(nullptr)
    {
        setInterceptsMouseClicks(true, true);
        setPaintingIsUnclipped(true);
        startTimerHz(30);

        const juce::String soloParamIDs[] = { "lowSolo", "lowMidSolo", "midHighSolo", "highSolo" };
        const juce::String muteParamIDs[] = { "lowMute", "lowMidMute", "midHighMute", "highMute" };
        const juce::String bypassParamIDs[] = { "lowBypass", "lowMidBypass", "midHighBypass", "highBypass" }; // <--- НОВОЕ

        juce::Colour buttonOffColour = ColorScheme::getToggleButtonOffColor().darker(0.3f); // Темнее, чем глобальные кнопки
        juce::Colour buttonOnColourLow = ColorScheme::getLowBandColor().brighter(0.2f);
        juce::Colour buttonOnColourLowMid = ColorScheme::getLowMidBandColor().brighter(0.2f);
        juce::Colour buttonOnColourMidHigh = ColorScheme::getMidHighBandColor().brighter(0.2f);
        juce::Colour buttonOnColourHigh = ColorScheme::getHighBandAltColor().brighter(0.2f);
        juce::Colour textColourOff = ColorScheme::getToggleButtonOffTextColor().brighter(0.5f);
        juce::Colour textColourOn = ColorScheme::getToggleButtonOnTextColor(); // Обычно белый или черный

        // Массив "включенных" цветов для каждой полосы
        juce::Colour bandSpecificOnColours[] = {
            buttonOnColourLow, buttonOnColourLowMid, buttonOnColourMidHigh, buttonOnColourHigh
        };

        for (int i = 0; i < numBands; ++i)
        {
            auto setupSmallButton = [&](juce::TextButton& btn, const juce::String& text, const juce::String& paramID,
                std::unique_ptr<ButtonAttachment>& attachment, const juce::String& tooltip)
                {
                    btn.setButtonText(text);
                    btn.setClickingTogglesState(true);
                    btn.setColour(juce::TextButton::buttonColourId, buttonOffColour);
                    // Цвет для состояния "включено" будет зависеть от полосы
                    // Мы не можем установить его здесь один раз, так как эти кнопки (S, M, B)
                    // не меняют свой "основной" цвет в зависимости от *выбранной* полосы,
                    // а скорее их цвет "включено" может быть фиксированным или зависеть от *их собственной* полосы.
                    // Для простоты, сделаем цвет "включено" одним и тем же для всех S, M, B,
                    // но он будет ярче, чем выключенное состояние.
                    // Или можно использовать bandSpecificOnColours[i] для кнопки i-й полосы.
                    // Давайте попробуем цвет, специфичный для полосы, для состояния "включено".
                    btn.setColour(juce::TextButton::buttonOnColourId, bandSpecificOnColours[i].darker(0.2f)); // Немного темнее основного цвета полосы
                    btn.setColour(juce::TextButton::textColourOffId, textColourOff);
                    btn.setColour(juce::TextButton::textColourOnId, textColourOn);

                    // Уменьшаем шрифт для маленьких кнопок
                    //btn.setFont(juce::Font(10.0f, juce::Font::bold)); // Маленький жирный шрифт

                    addAndMakeVisible(btn);
                    attachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), paramID, btn);
                    btn.setTooltip(tooltip + juce::String(i + 1));
                    btn.setVisible(true); // Видимы всегда по умолчанию
                };

            setupSmallButton(soloButtons[i], "S", soloParamIDs[i], soloAttachments[i], "Solo Band ");
            soloButtons[i].setComponentID("OverlaySoloButton");
            setupSmallButton(muteButtons[i], "M", muteParamIDs[i], muteAttachments[i], "Mute Band ");
            muteButtons[i].setComponentID("OverlayMuteButton"); // <--- Устанавливаем ID
            setupSmallButton(bypassButtons[i], "B", bypassParamIDs[i], bypassAttachments[i], "Bypass Band ");
            bypassButtons[i].setComponentID("OverlayBypassButton"); // <--- Устанавливаем ID
        }

        gainPopupDisplay.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey.withAlpha(0.85f));
        gainPopupDisplay.setColour(juce::Label::textColourId, juce::Colours::white);
        gainPopupDisplay.setColour(juce::Label::outlineColourId, juce::Colours::lightgrey);
        gainPopupDisplay.setBorderSize({ 1, 1, 1, 1 });
        gainPopupDisplay.setJustificationType(juce::Justification::centred);
        gainPopupDisplay.setFont(14.0f);
        addChildComponent(gainPopupDisplay);
        gainPopupDisplay.setVisible(false);

        crossoverPopupDisplay.setColour(juce::Label::backgroundColourId, juce::Colours::darkslategrey.withAlpha(0.85f)); // Другой цвет для отличия
        crossoverPopupDisplay.setColour(juce::Label::textColourId, juce::Colours::white);
        crossoverPopupDisplay.setColour(juce::Label::outlineColourId, juce::Colours::lightgrey);
        crossoverPopupDisplay.setBorderSize({ 1, 1, 1, 1 });
        crossoverPopupDisplay.setJustificationType(juce::Justification::centred);
        crossoverPopupDisplay.setFont(14.0f);
        addChildComponent(crossoverPopupDisplay);
        crossoverPopupDisplay.setVisible(false);


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
                if (currentGainDragState == GainDraggingState::None && currentlyHoveredOrDraggedGainState == GainDraggingState::None) {
                    hideGainPopup();
                }
                if (currentCrossoverDragState == CrossoverDraggingState::None && currentlyHoveredCrossoverState == CrossoverHoverState::None) {
                    crossoverPopupDisplay.setVisible(false); // Скрываем pop-up кроссовера
                    currentlyHoveredOrDraggedCrossoverParam = nullptr;
                }
            }
        }
        // из-за автоматизации DAW или перетаскивания в этом же компоненте.
        positionBandControls(getGraphBounds());
        // Всегда перерисовываем, чтобы линии обновлялись, если параметр изменился извне
        // Это также обновит и маркеры Gain, если их параметры изменились
        needsRepaint = true;

        if (needsRepaint) {
            repaint();
        }
    }

    // --- НОВЫЙ МЕТОД для отображения Pop-up кроссовера ---
    void AnalyzerOverlay::showCrossoverPopup(const juce::MouseEvent* eventForPosition, float valueHz)
    {
        popupHideDelayFramesCounter = 0; // Останавливаем задержку на скрытие

        juce::String text;
        if (valueHz < 1000.0f)
            text = juce::String(valueHz, 0) + " Hz";
        else
            text = juce::String(valueHz / 1000.0f, 1) + " kHz";

        crossoverPopupDisplay.setText(text, juce::dontSendNotification);

        int textWidth = crossoverPopupDisplay.getFont().getStringWidth(text);
        int popupWidth = textWidth + 20;
        int popupHeight = 25;

        if (eventForPosition)
        {
            auto mousePos = eventForPosition->getPosition();
            int x = mousePos.x + 15;
            int y = mousePos.y - popupHeight - 10; // Выше курсора

            if (getParentComponent()) { /* ... логика ограничения родительским компонентом (как для gainPopupDisplay) ... */ }
            crossoverPopupDisplay.setBounds(x, y, popupWidth, popupHeight);
        }

        if (!crossoverPopupDisplay.isVisible()) {
            crossoverPopupDisplay.setVisible(true);
            // crossoverPopupDisplay.toFront(true);
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
        auto graphCentreY = graphBounds.getCentreY(); // Центральная Y для вертикального позиционирования

        int buttonWidth = 20;  // Меньше ширина
        int buttonHeight = 20; // Меньше высота (или сделать их квадратными)
        int verticalSpacingBetweenButtons = 3;
        float horizontalOffsetFromCrossoverLine = 6.0f;

        // Получаем X-координаты линий кроссоверов
        float crossoverLineX[] = {
            mapFreqToXLog(processorRef.lowMidCrossover->get(), leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            mapFreqToXLog(processorRef.midCrossover->get(),    leftGraphEdge, graphWidth, minLogFreq, maxLogFreq),
            mapFreqToXLog(processorRef.midHighCrossover->get(),leftGraphEdge, graphWidth, minLogFreq, maxLogFreq)
        };

        // Определяем X-координаты для групп кнопок каждой полосы
        // Кнопки для полосы i будут справа от линии кроссовера i-1 (или у левого края для первой полосы)
        // и слева от линии кроссовера i.
        // В Saturn они обычно привязаны к ПРАВОЙ границе полосы (т.е. к линии кроссовера, ее завершающей).

        float bandRightBoundaryX[numBands];
        bandRightBoundaryX[0] = crossoverLineX[0]; // Правая граница Low полосы = Low/L-Mid кроссовер
        bandRightBoundaryX[1] = crossoverLineX[1]; // Правая граница L-Mid полосы = L-Mid/M-High кроссовер
        bandRightBoundaryX[2] = crossoverLineX[2]; // Правая граница M-High полосы = M-High/High кроссовер
        bandRightBoundaryX[3] = graphBounds.getRight(); // Правая граница High полосы = край графика

        float bandLeftBoundaryX[numBands];
        bandLeftBoundaryX[0] = graphBounds.getX();
        bandLeftBoundaryX[1] = crossoverLineX[0];
        bandLeftBoundaryX[2] = crossoverLineX[1];
        bandLeftBoundaryX[3] = crossoverLineX[2];


        for (int i = 0; i < numBands; ++i)
        {
            // Горизонтальная позиция для группы кнопок этой полосы
            // Разместим их немного левее от ПРАВОЙ границы полосы (линии кроссовера)
            float buttonsAnchorX = bandRightBoundaryX[i] - horizontalOffsetFromCrossoverLine - buttonWidth;

            // Если это самая первая полоса, и ее правая граница (первый кроссовер) слишком близко к левому краю,
            // возможно, стоит сдвинуть кнопки немного правее от левого края полосы.
            if (i == 0 && buttonsAnchorX < bandLeftBoundaryX[i] + buttonWidth + horizontalOffsetFromCrossoverLine) {
                buttonsAnchorX = bandLeftBoundaryX[i] + horizontalOffsetFromCrossoverLine;
            }
            // Для последней полосы, если она слишком узкая, кнопки могут не поместиться.
            // Проверяем, чтобы кнопки не вылезали за левую границу текущей полосы
            if (buttonsAnchorX < bandLeftBoundaryX[i] + horizontalOffsetFromCrossoverLine && i > 0) {
                buttonsAnchorX = bandLeftBoundaryX[i] + horizontalOffsetFromCrossoverLine;
            }
            // И чтобы не вылезали за правую границу графика
            if (buttonsAnchorX + buttonWidth > graphBounds.getRight() - horizontalOffsetFromCrossoverLine) {
                buttonsAnchorX = graphBounds.getRight() - buttonWidth - horizontalOffsetFromCrossoverLine;
            }


            // Вертикальное позиционирование: например, центрируем группу кнопок по вертикали графика
            // или немного ниже центральной линии, как в Saturn 2.
            float totalButtonsColumnHeight = buttonHeight * 3 + verticalSpacingBetweenButtons * 2;
            float topButtonY = graphCentreY - graphCentreY/1.5f - totalButtonsColumnHeight / 2.0f;
            // Можно добавить смещение, если нужно: + verticalOffsetFromCenter;

            // Проверяем, видна ли полоса и достаточно ли она широка
            bool canShowButtons = true;
            if (bandRightBoundaryX[i] <= bandLeftBoundaryX[i] || // Полоса нулевой или отрицательной ширины
                bandRightBoundaryX[i] < leftGraphEdge + buttonWidth || // Правая граница слишком слева
                bandLeftBoundaryX[i] > graphBounds.getRight() - buttonWidth || // Левая граница слишком справа
                (bandRightBoundaryX[i] - bandLeftBoundaryX[i]) < buttonWidth + 2 * horizontalOffsetFromCrossoverLine) // Полоса слишком узкая
            {
                canShowButtons = false;
            }


            if (canShowButtons) {
                int currentButtonY = juce::roundToInt(topButtonY);

                bypassButtons[i].setBounds(juce::roundToInt(buttonsAnchorX),
                    currentButtonY,
                    buttonWidth, buttonHeight);
                currentButtonY += buttonHeight + verticalSpacingBetweenButtons;

                soloButtons[i].setBounds(juce::roundToInt(buttonsAnchorX),
                    currentButtonY,
                    buttonWidth, buttonHeight);
                currentButtonY += buttonHeight + verticalSpacingBetweenButtons;

                muteButtons[i].setBounds(juce::roundToInt(buttonsAnchorX),
                    currentButtonY,
                    buttonWidth, buttonHeight);

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
        const juce::String panParamIDs[] = { "lowPan", "lowMidPan", "midHighPan", "highPan" };
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
            juce::AudioParameterFloat* panParamForActiveBand = nullptr;
            if (i == activeBandIndex) { // Получаем панораму только для активной полосы
                auto* paramBasePan = processorRef.getAPVTS().getParameter(panParamIDs[i]);
                panParamForActiveBand = dynamic_cast<juce::AudioParameterFloat*>(paramBasePan);
            }
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

                float panValue = 0.0f; // Центр по умолчанию
                if (panParamForActiveBand) {
                    panValue = panParamForActiveBand->get(); // Значение от -1.0 (L) до 1.0 (R)
                }

                // Рассчитываем смещение для X-координаты верхней точки градиента
                // Максимальное смещение, например, половина ширины полосы
                float bandWidthCurrent = bandRightX - bandLeftX;
                float maxPanOffset = bandWidthCurrent * 0.4f; // 40% ширины полосы в каждую сторону
                float panXOffset = panValue * maxPanOffset;

                // Начальная точка градиента (внизу, у маркера Gain)
                Point<float> gradientStartPoint(bandLeftX + bandWidthCurrent / 2.0f, yPos);

                // Конечная точка градиента (вверху, смещенная по X в зависимости от панорамы)
                Point<float> gradientEndPoint(gradientStartPoint.x + panXOffset, top);

                // Ограничиваем конечную точку границами полосы, чтобы наклон не был слишком сильным
                gradientEndPoint.x = juce::jlimit(bandLeftX, bandRightX, gradientEndPoint.x);


                juce::ColourGradient gradient(baseColour.withAlpha(0.35f), // Цвет у начальной точки (внизу)
                    gradientStartPoint,
                    baseColour.withAlpha(0.0f),  // Прозрачный цвет у конечной точки (вверху)
                    gradientEndPoint,
                    false);                     // не радиальный

                g.setGradientFill(gradient);
                // Рисуем прямоугольник, покрывающий всю полосу по ширине и высоте графика
                g.fillRect(bandLeftX, top, bandWidthCurrent, height);
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
        if ((gainPopupDisplay.isVisible() && currentGainDragState == GainDraggingState::None) ||
            (crossoverPopupDisplay.isVisible() && currentCrossoverDragState == CrossoverDraggingState::None))
        {
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
            if (currentlyHoveredOrDraggedCrossoverParam) {
                showCrossoverPopup(&event, currentlyHoveredOrDraggedCrossoverParam->get());
                popupHideDelayFramesCounter = 0; // Не даем скрыться
            }

            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            startPopupHideDelay();
            return;
        }

        if (currentGainDragState != GainDraggingState::None) {
            // Во время перетаскивания Gain, pop-up обновляется в mouseDrag
            if (currentlyHoveredOrDraggedGainParam) {
                showGainPopup(&event, currentlyHoveredOrDraggedGainParam->get());
                popupHideDelayFramesCounter = 0;
            }
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            if (crossoverPopupDisplay.isVisible()) startPopupHideDelay();
            return;
        }

        auto graphBounds = getGraphBounds();
        auto gainInfo = getGainInfoAt(event, graphBounds); // Использует event.x, event.y
        GainDraggingState hoveredGainStateNow = gainInfo.first;
        juce::AudioParameterFloat* hoveredGainParamNow = gainInfo.second;

        CrossoverHoverState newCrossoverHover = CrossoverHoverState::None;
        float newTargetAlphaLocal = 0.0f;
        juce::AudioParameterFloat* hoveredCrossoverParamNow = nullptr; // Параметр кроссовера под мышью
        juce::MouseCursor cursor = juce::MouseCursor::NormalCursor;

        if (hoveredGainStateNow != GainDraggingState::None && hoveredGainParamNow != nullptr)
        {
            currentlyHoveredOrDraggedGainState = hoveredGainStateNow;
            currentlyHoveredOrDraggedGainParam = hoveredGainParamNow;
            showGainPopup(&event, hoveredGainParamNow->get());
            cursor = juce::MouseCursor::UpDownResizeCursor;

            // Скрываем Crossover pop-up и подсветку
            if (crossoverPopupDisplay.isVisible()) startPopupHideDelay();
            targetHighlightAlpha = 0.0f;
            currentCrossoverHoverState = CrossoverHoverState::None;
            currentlyHoveredCrossoverState = CrossoverHoverState::None;
            currentlyHoveredOrDraggedCrossoverParam = nullptr;
        }
        else // Не наведены на Gain, проверяем кроссоверы
        {
            if (currentlyHoveredOrDraggedGainState != GainDraggingState::None) { startPopupHideDelay(); } // Ушли с Gain
            currentlyHoveredOrDraggedGainState = GainDraggingState::None;
            currentlyHoveredOrDraggedGainParam = nullptr;

            if (graphBounds.contains(event.getPosition().toFloat())) {
                float mouseX = static_cast<float>(event.x);
                float lmX = mapFreqToXLog(processorRef.lowMidCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
                float mX = mapFreqToXLog(processorRef.midCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);
                float mhX = mapFreqToXLog(processorRef.midHighCrossover->get(), graphBounds.getX(), graphBounds.getWidth(), minLogFreq, maxLogFreq);

                if (std::abs(mouseX - lmX) < dragToleranceX) { newCrossoverHover = CrossoverHoverState::HoveringLowMid; hoveredCrossoverParamNow = processorRef.lowMidCrossover; }
                else if (std::abs(mouseX - mX) < dragToleranceX) { newCrossoverHover = CrossoverHoverState::HoveringMid; hoveredCrossoverParamNow = processorRef.midCrossover; }
                else if (std::abs(mouseX - mhX) < dragToleranceX) { newCrossoverHover = CrossoverHoverState::HoveringMidHigh; hoveredCrossoverParamNow = processorRef.midHighCrossover; }

                if (newCrossoverHover != CrossoverHoverState::None && hoveredCrossoverParamNow != nullptr) {
                    currentlyHoveredCrossoverState = newCrossoverHover;
                    currentlyHoveredOrDraggedCrossoverParam = hoveredCrossoverParamNow;
                    showCrossoverPopup(&event, hoveredCrossoverParamNow->get());
                    newTargetAlphaLocal = targetAlphaValue;
                    cursor = juce::MouseCursor::LeftRightResizeCursor;
                }
                else {
                    if (currentlyHoveredCrossoverState != CrossoverHoverState::None) { startPopupHideDelay(); } // Ушли с кроссовера
                    currentlyHoveredCrossoverState = CrossoverHoverState::None;
                    currentlyHoveredOrDraggedCrossoverParam = nullptr;
                }
            }
            else { // Мышь вне графика
                if (currentlyHoveredCrossoverState != CrossoverHoverState::None) { startPopupHideDelay(); }
                if (currentlyHoveredOrDraggedGainState != GainDraggingState::None) { startPopupHideDelay(); }
                currentlyHoveredCrossoverState = CrossoverHoverState::None; currentlyHoveredOrDraggedCrossoverParam = nullptr;
                currentlyHoveredOrDraggedGainState = GainDraggingState::None; currentlyHoveredOrDraggedGainParam = nullptr;
            }
        }

        setMouseCursor(cursor);
        if (newCrossoverHover != CrossoverHoverState::None) { lastCrossoverHoverStateForColor = newCrossoverHover; }
        if (newCrossoverHover != currentCrossoverHoverState && hoveredGainStateNow == GainDraggingState::None) { /* Обновляем состояние наведения кроссовера только если не наведены на гейн */ }

        if (hoveredGainStateNow == GainDraggingState::None && !juce::approximatelyEqual(newTargetAlphaLocal, targetHighlightAlpha)) {
            targetHighlightAlpha = newTargetAlphaLocal;
        }
        else if (hoveredGainStateNow != GainDraggingState::None) {
            targetHighlightAlpha = 0.0f;
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
            currentlyHoveredOrDraggedCrossoverParam = paramToChange; // Запоминаем для mouseDrag
            currentlyHoveredCrossoverState = CrossoverHoverState::None; // Сбрасываем, т.к. начинаем драг
            // (хотя lastCrossoverHoverStateForColor уже должен быть установлен)
            showCrossoverPopup(&event, paramToChange->get()); // Показываем pop-up для кроссовера
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
            hideGainPopup();
            if (crossoverPopupDisplay.isVisible()) startPopupHideDelay();
        }
        else {
            currentCrossoverDragState = CrossoverDraggingState::None;
            currentGainDragState = GainDraggingState::None;
            currentlyHoveredOrDraggedGainState = GainDraggingState::None;
            currentlyHoveredOrDraggedGainParam = nullptr;
            targetHighlightAlpha = 0.0f; currentCrossoverHoverState = CrossoverHoverState::None;
            hideGainPopup();
            if (crossoverPopupDisplay.isVisible()) startPopupHideDelay();
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
        bool positionChanged = false;
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
            float oldValGain = currentlyHoveredOrDraggedGainParam->get();
            // ПРОВЕРЯЕМ ПОРЯДОК АРГУМЕНТОВ ЗДЕСЬ:
            if (std::abs(oldValGain - newGainDb) > 0.01f) { // значение1, значение2, допуск
                currentlyHoveredOrDraggedGainParam->setValueNotifyingHost(currentlyHoveredOrDraggedGainParam->getNormalisableRange().convertTo0to1(newGainDb));
                positionChanged = true;
            }
            showGainPopup(&event, newGainDb); // Обновляем pop-up в реальном времени
            popupHideDelayFramesCounter = 0; // Не даем pop-up скрыться во время драга
        }

        else if (currentCrossoverDragState != CrossoverDraggingState::None) {
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
                if (std::abs(paramToUpdateXOver->get() - finalFreq )> 0.1f) { // Проверка на изменение
                    paramToUpdateXOver->setValueNotifyingHost(paramToUpdateXOver->getNormalisableRange().convertTo0to1(finalFreq));
                    positionChanged = true; // X-координаты границ полос изменились
                }

            }
            targetHighlightAlpha = targetAlphaValue;
            float newFreqVal = currentlyHoveredOrDraggedCrossoverParam->get();
            showCrossoverPopup(&event, newFreqVal); // Обновляем pop-up для кроссовера
            popupHideDelayFramesCounter = 0;
            targetHighlightAlpha = targetAlphaValue;
        }
        else
        {
            return;
        }

        if (positionChanged) {
            positionBandControls(graphBounds); // Обновляем позиции кнопок
            repaint(); // Перерисовываем, чтобы показать изменения
        }
    }

    void AnalyzerOverlay::mouseUp(const juce::MouseEvent& event)
    {
        bool wasDraggingSomething = false;
        if (currentGainDragState != GainDraggingState::None) {
            if (currentlyHoveredOrDraggedGainParam) { // Проверка на nullptr
                currentlyHoveredOrDraggedGainParam->endChangeGesture();
                wasDraggingSomething = true;

            }
            // currentlyHoveredOrDraggedGainState и currentlyHoveredOrDraggedGainParam НЕ сбрасываем здесь,
            // чтобы mouseMove мог решить, нужно ли запускать таймер скрытия, если мышь осталась над маркером.
            startPopupHideDelay();
            currentGainDragState = GainDraggingState::None; // Сбрасываем только состояние перетаскивания
            mouseMove(event);
            return;
        }
        currentGainDragState = GainDraggingState::None;

        if (currentCrossoverDragState != CrossoverDraggingState::None && currentlyHoveredOrDraggedCrossoverParam != nullptr) {
            currentlyHoveredOrDraggedCrossoverParam->endChangeGesture();
            wasDraggingSomething = true;
        }
        currentCrossoverDragState = CrossoverDraggingState::None; // Сбрасываем
        if (wasDraggingSomething) {
            startPopupHideDelay(); // Запускаем таймер на скрытие для обоих pop-up
        }
        mouseMove(event);
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
