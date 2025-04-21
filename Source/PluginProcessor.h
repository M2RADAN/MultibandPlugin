#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic> // Для std::atomic

//==============================================================================
/**
    Класс аудио процессора для MBRP (Multiband Panner)
*/
class MBRPAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    MBRPAudioProcessor();
    ~MBRPAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; }

private:
    //==============================================================================
    juce::ScopedPointer<juce::AudioProcessorValueTreeState> apvts;

    // --- Фильтры Linkwitz-Riley 2-го порядка (12 дБ/окт) ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;

    // Фильтры для разделения НЧ / (СЧ+ВЧ)
    Filter leftLowMidLPF, rightLowMidLPF; // Low Pass < lowMidXover
    Filter leftLowMidHPF, rightLowMidHPF; // High Pass > lowMidXover

    // Фильтры для разделения СЧ / ВЧ (применяются к выходу HPF выше)
    Filter leftMidHighLPF, rightMidHighLPF; // Low Pass < midHighXover
    Filter leftMidHighHPF, rightMidHighHPF; // High Pass > midHighXover


    // --- Параметры панорамы (атомарные для потокобезопасности) ---
    std::atomic<float> leftLowGain{ 1.f };
    std::atomic<float> rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f };
    std::atomic<float> rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f };
    std::atomic<float> rightHighGain{ 1.f };

    // --- Временные буферы для обработки ---
    juce::AudioBuffer<float> intermediateBuffer1; // Для (СЧ+ВЧ) после первого каскада
    juce::AudioBuffer<float> lowBandBuffer;       // Для финальной НЧ полосы
    juce::AudioBuffer<float> midBandBuffer;       // Для финальной СЧ полосы
    juce::AudioBuffer<float> highBandBuffer;      // Для финальной ВЧ полосы


    float lastSampleRate = 44100.0f; // Храним sample rate

    // --- Функции обновления ---
    void updateParameters(); // Обновляет фильтры и гейны панорамы

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};