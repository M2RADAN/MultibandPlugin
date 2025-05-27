#include "RotarySliderWithLabels.h"
#include "Utilities.h"       // Assuming getValString, truncateKiloValue are here
#include "LookAndFeel.h"     // For ColorScheme
#include <juce_gui_basics/juce_gui_basics.h> // For TextLayout

// --- Helper function to get text width using TextLayout ---
// (Can be moved to Utilities.h if used elsewhere)
static float getTextLayoutWidth(const juce::String& text, const juce::Font& font)
{
    juce::TextLayout textLayout;
    juce::AttributedString attrString(text);
    attrString.setFont(font);
    textLayout.createLayout(attrString, 10000.0f); // Large width to get natural size
    return textLayout.getWidth();
}
// ----------------------------------------------------

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics& g)
{
    using namespace juce;
    auto localBounds = getLocalBounds();
    auto sliderActualBounds = getSliderBounds();

    // --- LookAndFeel нарисует сам слайдер ---
    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;
    auto range = getRange();

    getLookAndFeel().drawRotarySlider(g,
        sliderActualBounds.getX(),
        sliderActualBounds.getY(),
        sliderActualBounds.getWidth(),
        sliderActualBounds.getHeight(),
        (float)jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng,
        endAng,
        *this);

    // --- Рисуем заголовок (title) ---
    if (getName().isNotEmpty())
    {
        juce::Font titleFont("K2D", 16.0f, juce::Font::plain);
        g.setColour(ColorScheme::getSliderTrackColor());
        g.setFont(titleFont);
        Rectangle<int> titleArea;
        if (titleAboveSlider) {
            titleArea = localBounds.removeFromTop(getTitleHeight()).reduced(0, 2);
            g.drawText(getName(), titleArea, Justification::centredBottom, false);
        }
        else {
            titleArea = localBounds.removeFromBottom(getTitleHeight()).reduced(0, 2);
            g.drawText(getName(), titleArea, Justification::centredTop, false);
        }
    }

    // --- Отрисовка внешних меток диапазона (L/C/R для Pan или 0%/100% для других) ---
    if (drawRangeLabels && !labels.isEmpty())
    {
        auto center = sliderActualBounds.toFloat().getCentre();
        // Шрифт для меток L/C/R (и других меток диапазона)
        auto rangeLabelFont = juce::Font(juce::FontOptions(12.0f)); // Можно сделать поменьше, если нужно
        g.setFont(rangeLabelFont);
        g.setColour(ColorScheme::getRotarySliderLabelColor());

        bool isPan = (param != nullptr && param->getName(100).containsIgnoreCase("Pan"));

        for (const auto& labelPos : labels)
        {
            auto pos = labelPos.pos; // 0.0 (min), 0.5 (center), 1.0 (max)
            auto str = labelPos.label;
            float textWidth = rangeLabelFont.getStringWidthFloat(str);
            Rectangle<float> textBounds(textWidth + 4.0f, 14.0f); // Ширина с небольшим запасом, высота 14

            if (isPan) // Специальное позиционирование для L/C/R у Pan слайдера
            {
                // Предполагаем, что заголовок "PAN" уже нарисован под слайдером
                // Метки L и R по бокам от нижней части слайдера
                float yPosPanLabels = sliderActualBounds.getBottom() + 2; // Чуть ниже круга слайдера

                if (str.equalsIgnoreCase("L")) {
                    textBounds.setCentre(sliderActualBounds.getX() + textBounds.getWidth() / 2.0f, yPosPanLabels);
                }
                else if (str.equalsIgnoreCase("R")) {
                    textBounds.setCentre(sliderActualBounds.getRight() - textBounds.getWidth() / 2.0f, yPosPanLabels);
                }
                else if (str.equalsIgnoreCase("C")) {
                    // Метку "C" можно не рисовать, если значение "C" отображается внутри слайдера
                    // Если все же нужно, можно разместить ее по центру под слайдером, но выше заголовка "PAN"
                    // или немного сдвинуть L и R, чтобы освободить место.
                    // Пока пропустим отрисовку "C" как внешней метки, если getDisplayString() ее показывает.
                    if (getDisplayString().equalsIgnoreCase("C")) continue; // Не рисуем внешнюю "C", если она уже в центре
                    textBounds.setCentre(center.x, yPosPanLabels);
                }
                else {
                    // Для других меток у Pan (если вдруг будут) используем стандартное размещение
                    auto ang = jmap(pos, 0.0f, 1.0f, startAng, endAng);
                    float radiusForOtherLabels = sliderActualBounds.getWidth() * 0.6f; // Примерный радиус
                    textBounds.setCentre(center.getPointOnCircumference(radiusForOtherLabels, ang));
                }
            }
            else // Стандартное позиционирование для других слайдеров (0%, 100%)
            {
                // По бокам от слайдера, если заголовок снизу
                // Или по кругу, если заголовок сверху (как было раньше)
                if (!titleAboveSlider) {
                    float yPosRangeLabels = sliderActualBounds.getBottom() - textBounds.getHeight() / 2 - 5; // Немного выше низа слайдера
                    if (pos == 0.0f) { // Левая метка
                        textBounds.setCentre(sliderActualBounds.getX() - textWidth / 2 - 5, yPosRangeLabels);
                    }
                    else if (pos == 1.0f) { // Правая метка
                        textBounds.setCentre(sliderActualBounds.getRight() + textWidth / 2 + 5, yPosRangeLabels);
                    }
                    else { // Для других меток (например, центральной, если бы она была)
                        auto ang = jmap(pos, 0.0f, 1.0f, startAng, endAng);
                        float radiusForOtherLabels = sliderActualBounds.getWidth() * 0.6f;
                        textBounds.setCentre(center.getPointOnCircumference(radiusForOtherLabels, ang));
                    }
                }
                else { // Заголовок сверху, метки по кругу (старое поведение)
                    auto ang = jmap(pos, 0.0f, 1.0f, startAng, endAng);
                    float radiusForLabels = sliderActualBounds.getWidth() * 0.5f + 10.0f; // Чуть дальше от круга
                    textBounds.setCentre(center.getPointOnCircumference(radiusForLabels, ang));
                }
            }

            // Ограничение, чтобы метки не вылезали за пределы компонента
            textBounds.setX(juce::jmax(0.0f, textBounds.getX()));
            textBounds.setRight(juce::jmin((float)getWidth(), textBounds.getRight()));
            textBounds.setY(juce::jmax(0.0f, textBounds.getY()));
            textBounds.setBottom(juce::jmin((float)getHeight(), textBounds.getBottom()));

            g.drawFittedText(str, textBounds.toNearestInt(), Justification::centred, 1);
        }
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    if (getName().isNotEmpty()) { // Если есть заголовок
        if (titleAboveSlider) {
            bounds.removeFromTop(getTitleHeight());
        }
        else {
            bounds.removeFromBottom(getTitleHeight());
        }
    }

    // Убираем место для внешних меток диапазона, если они рисуются и заголовок НЕ снизу
    // (если заголовок снизу, предполагаем, что метки диапазона рисуются по бокам и не требуют доп. места по вертикали)
    int bottomMarginForRangeLabels = 0;
    if (drawRangeLabels && !labels.isEmpty() && titleAboveSlider) {
        // Примерный отступ, если метки рисуются вокруг и заголовок сверху
        bottomMarginForRangeLabels = 15; // Можно сделать зависимым от getTextHeight или другого значения
    }
    bounds.removeFromBottom(bottomMarginForRangeLabels);

    int size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    return bounds.withSizeKeepingCentre(size, size);
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    // --- Специальная обработка для Pan ---
    // Проверяем по имени параметра или по суффиксу, если он уникален для Pan
    bool isPanSlider = (param != nullptr && param->getName(100).containsIgnoreCase("Pan"));
    // Или можно проверить по ID, если они известны:
    // bool isPanSlider = (param != nullptr && (param->paramID == "lowPan" || param->paramID == "midPan" || param->paramID == "highPan"));



    if (isPanSlider)
    {
        float value = static_cast<float>(getValue()); // Текущее значение от -1.0 до 1.0

        // Проверяем на центр с небольшим допуском
        if (std::abs(value) < 0.01f) {
            return "C"; // Центр
        }
        else {
            // Вычисляем процент и направление
            int percentage = static_cast<int>(std::round(std::abs(value) * 100.0f));
            if (value < 0.0f) {
                return "L" + juce::String(percentage); // Лево
            }
            else {
                return "R" + juce::String(percentage); // Право
            }
        }
    }
    // --- Конец специальной обработки для Pan ---


    // --- Стандартная обработка для других типов параметров ---
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    // Для RangedAudioParameter (включая AudioParameterFloat, AudioParameterInt)
    if (param != nullptr) // Убедимся, что параметр установлен
    {
        // Используем метод параметра для получения строки значения.
        // Этот метод обычно уже учитывает диапазон, суффикс и форматирование.
        // Второй аргумент (maximumStringLength) не так важен для отображения внутри слайдера.
        juce::String text = param->getText(param->getValue(), 0); // 0 для "неограниченной" длины

        // Если суффикс параметра пустой (например, для панорамы),
        // а мы задали суффикс для RotarySliderWithLabels (например, "%"), то добавим его.
        // Но для параметров реверба (Wet, Space, Distance) параметр уже должен иметь суффикс "%".
        // Для Pre-Delay параметр должен иметь "ms".
        // Если мы задали суффикс в конструкторе RotarySliderWithLabels, и он отличается от суффикса параметра,
        // или если у параметра нет суффикса, то наш suffix из RotarySliderWithLabels может не отобразиться.

        // Давайте проверим, был ли суффикс уже добавлен getText()
        // Это немного хрупко, но для процентов и ms должно сработать.
        bool suffixAlreadyPresent = false;
        if (suffix.isNotEmpty()) {
            if (text.endsWithIgnoreCase(suffix) || text.endsWithIgnoreCase(suffix.trim())) {
                suffixAlreadyPresent = true;
            }
            // Особый случай для частот с 'k'
            if (suffix.containsIgnoreCase("hz") && (text.endsWithIgnoreCase("khz") || text.endsWithIgnoreCase("k hz"))) {
                suffixAlreadyPresent = true;
            }
        }

        // Если наш suffix не пустой и он ЕЩЕ НЕ присутствует в строке от параметра, добавляем его.
        if (suffix.isNotEmpty() && !suffixAlreadyPresent) {
            // Удаляем возможный числовой суффикс, если он есть, перед добавлением нашего
            // (например, если параметр дал "100.0", а наш суффикс "%", хотим "100 %", а не "100.0 %")
            // Это упрощение; более надежно было бы парсить число.
            if (text.containsChar('.') && text.endsWithChar('0'))
            {
                int dotPos = text.lastIndexOfChar('.');
                bool allZerosAfterDot = true;
                for (int i = dotPos + 1; i < text.length(); ++i) {
                    if (text[i] != '0') {
                        allZerosAfterDot = false;
                        break;
                    }
                }
                if (allZerosAfterDot) {
                    text = text.substring(0, dotPos);
                }
            }
            text += suffix; // Добавляем наш суффикс, если он нужен
        }
        return text;
    }

    // Если параметр не установлен или неизвестного типа, возвращаем необработанное значение
    // (этот блок теперь менее вероятен, если param всегда устанавливается)
    jassert(param != nullptr && "Parameter is null in RotarySliderWithLabels::getDisplayString");
    float value = static_cast<float>(getValue()); // Нормализованное значение
    // Здесь можно попытаться денормализовать вручную, если есть доступ к диапазону,
    // но лучше полагаться на param->getText().
    return juce::String(value, 2) + suffix; // Запасной вариант
}

void RotarySliderWithLabels::changeParam(juce::RangedAudioParameter* p)
{
    // Update the internal parameter pointer and repaint
    param = p;
    repaint();
}
//==============================================================================
// Implementation for RatioSlider (if used)
juce::String RatioSlider::getDisplayString() const
{
    auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param);
    jassert(choiceParam != nullptr); // Ensure it's a choice parameter

    auto currentChoice = choiceParam->getCurrentChoiceName();
    // Remove ".0" if present (e.g., "4.0" -> "4")
    if (currentChoice.contains(".0"))
        currentChoice = currentChoice.upToFirstOccurrenceOf(".", false, false);

    // Append ":1" for ratio display
    currentChoice << ":1";

    return currentChoice;
}
