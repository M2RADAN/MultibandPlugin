#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h> // Убедимся, что dsp модуль включен

//==============================================================================
class MultibandPannerAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    MultibandPannerAudioProcessor();
    ~MultibandPannerAudioProcessor() override;

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

    // --- Фильтры Linkwitz-Riley для кроссовера ---
    // Нужны пары LP/HP для каждой частоты среза и для каждого канала
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    // Полоса НЧ (Low Pass < LowMid Xover)
    Filter leftLowLPF, rightLowLPF;
    // Полоса СЧ (High Pass > LowMid Xover, Low Pass < MidHigh Xover)
    Filter leftMidHPF, rightMidHPF; // Фильтры для нижней границы СЧ
    Filter leftMidLPF, rightMidLPF; // Фильтры для верхней границы СЧ
    // Полоса ВЧ (High Pass > MidHigh Xover)
    Filter leftHighHPF, rightHighHPF;

    // --- Параметры и гейны панорамы ---
    // Храним рассчитанные гейны, чтобы не вычислять тригонометрию каждый семпл
    std::atomic<float> leftLowGain{ 1.f };
    std::atomic<float> rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f };
    std::atomic<float> rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f };
    std::atomic<float> rightHighGain{ 1.f };

    // --- Временные буферы для полос ---
    // Потребуются для хранения разделенных сигналов перед панорамированием
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;
    // (Альтернатива - более сложная обработка на лету без доп. буферов)

    float lastSampleRate = 44100.0f; // Для инициализации и prepareToPlay

    // --- Функции обновления ---
    void updateParameters(); // Обновляет фильтры и гейны панорамы

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultibandPannerAudioProcessor)
};