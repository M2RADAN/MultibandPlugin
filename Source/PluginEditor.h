#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h" // Включает MBRPAudioProcessor

// --- ВКЛЮЧАЕМ КОМПОНЕНТЫ GUI ---
// Убедись, что пути и пространства имен верны, и файлы скопированы
#include "GUI/LookAndFeel.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/CustomButtons.h"
#include "GUI/BandSelectControls.h"
#include "GUI/RotarySliderWithLabels.h"
// -------------------------------

// --- ControlBar ---
struct ControlBar : juce::Component
{
    ControlBar();
    void resized() override;
    AnalyzerButton analyzerButton; // Глобальное пространство имен
};
// ----------------

//==============================================================================
class MBRPAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Timer
{
public:
    explicit MBRPAudioProcessorEditor(MBRPAudioProcessor&);
    ~MBRPAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    LookAndFeel lnf; // Глобальное пространство имен
    MBRPAudioProcessor& processorRef;

    // --- Компоненты GUI ---
    ControlBar controlBar;
    MBRP_GUI::SpectrumAnalyzer analyzer;
    MBRP_GUI::AnalyzerOverlay analyzerOverlay;
    MBRP_GUI::BandSelectControls bandSelectControls;
    // ----------------------

    // Контролы панорамы/кроссовера
    juce::Slider lowMidCrossoverSlider, midHighCrossoverSlider;
    juce::Label lowMidCrossoverLabel, midHighCrossoverLabel;

    // --- ИЗМЕНЕНО: Только один набор контролов панорамы ---
    RotarySliderWithLabels panSlider;
    juce::Label panLabel;
    // ---------------------------------------------------

    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttachment = APVTS::SliderAttachment;

    // Аттачменты для кроссоверов (остаются)
    SliderAttachment lowMidCrossoverAttachment, midHighCrossoverAttachment;

    // --- ИЗМЕНЕНО: Только один аттачмент для панорамы (будет пересоздаваться) ---
    std::unique_ptr<SliderAttachment> panAttachment; // Используем unique_ptr для легкого пересоздания
    // ----------------------------------------------------------------------

    // --- ДОБАВЛЕНО: Функция для обновления аттачмента панорамы ---
    void updatePanAttachment(int bandIndex);
    // ---------------------------------------------------------

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessorEditor)
};