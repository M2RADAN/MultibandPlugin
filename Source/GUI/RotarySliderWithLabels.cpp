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

    auto startAng = degreesToRadians(180.f + 45.f); // Start angle for the rotary arc
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi; // End angle
    auto range = getRange();
    auto sliderBounds = getSliderBounds(); // Get bounds for the rotary part
    auto localBounds = getLocalBounds();   // Get total component bounds

    //// --- Draw Title ---
    //// Use FontOptions
    //auto titleFont = juce::Font(juce::FontOptions(static_cast<float>(getTextHeight() + 1), Font::bold)); // Slightly larger/bold
    //g.setColour(ColorScheme::getTitleColor());
    //g.setFont(titleFont);
    //// Draw title centered at the top
    //g.drawFittedText(getName(),
    //    localBounds.removeFromTop(getTextHeight() + 4), // Allocate space for title
    //    Justification::centredBottom, 1);
    if (getName().isNotEmpty()) // Рисуем, только если имя (заголовок) установлено
    {
        auto titleFont = juce::Font(juce::FontOptions(static_cast<float>(getTextHeight()), Font::plain)); // Шрифт для заголовка
        g.setColour(ColorScheme::getTextColor()); // Используем основной цвет текста
        // Область для заголовка - верхняя часть localBounds
        // Высота getTextHeight() + небольшой отступ снизу (например, 2 пикселя)
        g.drawFittedText(getName(),
            localBounds.removeFromTop(getTextHeight() + 2).reduced(2, 0), // Уменьшаем по горизонтали, чтобы не прилипало к краям
            Justification::centredBottom, 1);
    }
    // --- Delegate rotary drawing to LookAndFeel ---
    getLookAndFeel().drawRotarySlider(g,
        sliderBounds.getX(),
        sliderBounds.getY(),
        sliderBounds.getWidth(),
        sliderBounds.getHeight(),
        // Map value to proportional position [0, 1]
        (float)jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng,
        endAng,
        *this);

    // --- Draw Min/Max/Custom Labels around the slider ---
    if (!labels.isEmpty())
    {
        auto center = sliderBounds.toFloat().getCentre();
        auto radius = sliderBounds.getWidth() * 0.5f;
        g.setColour(ColorScheme::getTextColor()); // Используем основной цвет текста
        auto labelFont = juce::Font(juce::FontOptions(static_cast<float>(getTextHeight() - 2)));
        g.setFont(labelFont);
        // ... (остальная логика отрисовки labels L/C/R) ...
        for (const auto& labelPos : labels)
        {

            auto pos = labelPos.pos; // Normalized position (0.0 to 1.0)
            jassert(0.f <= pos && pos <= 1.f); // Ensure valid position
            auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);
            auto labelCenter = center.getPointOnCircumference(radius + static_cast<float>(getTextHeight()) * 1.0f, ang); // Подвинем чуть ближе

            auto str = labelPos.label;
            float textWidth = getTextLayoutWidth(str, labelFont);
            Rectangle<float> textBounds;
            textBounds.setSize(textWidth + 2.0f, static_cast<float>(getTextHeight())); // Добавим запас по ширине
            textBounds.setCentre(labelCenter);
            g.drawFittedText(str, textBounds.toNearestInt(), Justification::centred, 1);
        }
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    // Убираем место для заголовка сверху
    if (getName().isNotEmpty()) {
        bounds.removeFromTop(getTextHeight() + 2); // Высота заголовка + отступ
    }
    // Убираем место для текстового поля снизу, если оно есть и видимо
    if (getTextBoxPosition() != NoTextBox && isTextBoxEditable()) { // Проверяем, что TextBox реально есть
        bounds.removeFromBottom(getTextBoxHeight());
    }

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
