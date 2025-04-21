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

    // --- Фильтры (Имена для ясности, как в компрессоре) ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;

    // Каскад 1 (Частота Low/Mid Crossover)
    Filter leftLPF1, rightLPF1;  // Low Pass для НЧ
    Filter leftHPF1, rightHPF1;  // High Pass для (СЧ+ВЧ)

    // Каскад 2 (Частота Mid/High Crossover)
    Filter leftLPF2, rightLPF2;  // Low Pass для СЧ (применяется к выходу HPF1)
    Filter leftHPF2, rightHPF2;  // High Pass для ВЧ (применяется к выходу HPF1)
    Filter leftAPF2, rightAPF2;  // All Pass для НЧ (компенсация фазы 2-го каскада)


    // --- Параметры панорамы (атомарные для потокобезопасности) ---
    std::atomic<float> leftLowGain{ 1.f };
    std::atomic<float> rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f };
    std::atomic<float> rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f };
    std::atomic<float> rightHighGain{ 1.f };

    // --- Буферы для обработки полос ---
    juce::AudioBuffer<float> lowBandBuffer;       // Для финальной НЧ полосы
    juce::AudioBuffer<float> midBandBuffer;       // Для финальной СЧ полосы
    juce::AudioBuffer<float> highBandBuffer;      // Для финальной ВЧ полосы
    juce::AudioBuffer<float> hpf1OutputBuffer;    // Временный для хранения выхода HPF1


    float lastSampleRate = 44100.0f; // Храним sample rate

    // --- Функции обновления ---
    void updateParameters(); // Обновляет фильтры и гейны панорамы

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};