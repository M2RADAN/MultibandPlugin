#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h> // <<< ДОБАВИТЬ
#include <atomic>
#include <memory>

//==============================================================================
class MBRPAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    MBRPAudioProcessor();
    ~MBRPAudioProcessor() override;

    // --- Стандартные методы AudioProcessor ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    double getSampleRate() const { return lastSampleRate; }


    // --- APVTS и Параметры ---
    using APVTS = juce::AudioProcessorValueTreeState;
    static APVTS::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; } // Getter

    juce::AudioParameterFloat* lowMidCrossover{ nullptr };
    juce::AudioParameterFloat* midCrossover{ nullptr };
    juce::AudioParameterFloat* midHighCrossover{ nullptr };

    // --- Атомарные указатели на параметры реверба ---
    std::atomic<float>* lowWetParam = nullptr;
    std::atomic<float>* lowSpaceParam = nullptr;
    std::atomic<float>* lowDistanceParam = nullptr;
    std::atomic<float>* lowDelayParam = nullptr;

    std::atomic<float>* lowMidWetParam = nullptr;
    std::atomic<float>* lowMidSpaceParam = nullptr;
    std::atomic<float>* lowMidDistanceParam = nullptr;
    std::atomic<float>* lowMidDelayParam = nullptr;

    std::atomic<float>* midHighWetParam = nullptr;
    std::atomic<float>* midHighSpaceParam = nullptr;
    std::atomic<float>* midHighDistanceParam = nullptr;
    std::atomic<float>* midHighDelayParam = nullptr;

    std::atomic<float>* highWetParam = nullptr;
    std::atomic<float>* highSpaceParam = nullptr;
    std::atomic<float>* highDistanceParam = nullptr;
    std::atomic<float>* highDelayParam = nullptr;

    // Параметры громкости, Bypass, Solo, Mute (атомарные указатели) ---
    std::atomic<float>* lowGainParam = nullptr;
    std::atomic<float>* lowMidGainParam = nullptr;
    std::atomic<float>* midHighGainParam = nullptr;
    std::atomic<float>* highGainParam = nullptr;

    juce::AudioParameterBool* lowBypassParam = nullptr; 
    juce::AudioParameterBool* lowMidBypassParam = nullptr;
    juce::AudioParameterBool* midHighBypassParam = nullptr;
    juce::AudioParameterBool* highBypassParam = nullptr;

    juce::AudioParameterBool* lowSoloParam = nullptr;
    juce::AudioParameterBool* lowMidSoloParam = nullptr;
    juce::AudioParameterBool* midHighSoloParam = nullptr;
    juce::AudioParameterBool* highSoloParam = nullptr;

    juce::AudioParameterBool* lowMuteParam = nullptr;
    juce::AudioParameterBool* lowMidMuteParam = nullptr;
    juce::AudioParameterBool* midHighMuteParam = nullptr;
    juce::AudioParameterBool* highMuteParam = nullptr;

    // Listener для параметров
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    static constexpr float MIN_CROSSOVER_SEPARATION = 10.0f;


    // --- Члены для Анализатора Спектра ---
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    std::atomic<bool> nextFFTBlockReady{ false };
    juce::AbstractFifo abstractFifoInput{ fftSize * 2 };
    juce::AudioBuffer<float> audioFifoInput;
    juce::AbstractFifo abstractFifoOutput{ fftSize * 2 };
    juce::AudioBuffer<float> audioFifoOutput;
    void setCopyToFifo(bool _copyToFifo);

    bool isCopyToFifoEnabled() const { return copyToFifo.load(); }
    juce::Point<int> getSavedEditorSize() const { return editorSize; }
    void setSavedEditorSize(const juce::Point<int>& size) { editorSize = size; }

    juce::AudioParameterBool* bypassParameter{ nullptr };
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;

    // --- Фильтры и обработка ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter leftLowMidLPF, rightLowMidLPF;   // LPF для отделения Low от LowMid+MidHigh+High
    Filter leftMidLPF, rightMidLPF;         // LPF для отделения Low+LowMid от MidHigh+High (на основе входа)
    Filter leftMidHighLPF, rightMidHighLPF; // LPF для отделения Low+LowMid+MidHigh от High (на основе входа)


    std::atomic<float> leftLowPanGain{ 1.f }, rightLowPanGain{ 1.f };
    std::atomic<float> leftLowMidPanGain{ 1.f }, rightLowMidPanGain{ 1.f };
    std::atomic<float> leftMidHighPanGain{ 1.f }, rightMidHighPanGain{ 1.f };
    std::atomic<float> leftHighPanGain{ 1.f }, rightHighPanGain{ 1.f };

    // Буферы для разделения на полосы
    juce::AudioBuffer<float> lowBandBuffer;      // Полоса Low
    juce::AudioBuffer<float> lowMidBandBuffer;   // Полоса Low-Mid
    juce::AudioBuffer<float> midHighBandBuffer;  // Полоса Mid-High
    juce::AudioBuffer<float> highBandBuffer;     // Полоса High

    // Вспомогательные буферы для фильтрации
    juce::AudioBuffer<float> tempFilterBuffer1; // Для результата LPF(midCrossover)
    juce::AudioBuffer<float> tempFilterBuffer2; // Для результата LPF(midHighCrossover)


    // --- Управление FIFO ---
    std::atomic<bool> copyToFifo{ false };
    void pushNextSampleToFifo(const juce::AudioBuffer<float>& buffer, int startChannel, int numChannels,
        juce::AbstractFifo& absFifo, juce::AudioBuffer<float>& fifo);

    float lastSampleRate = 44100.0f;
    juce::Point<int> editorSize = { 2000, 1020 }; // Увеличил высоту по умолчанию

    void updateParameters();

    // --- DSP объекты реверберации для каждой полосы ---
    juce::dsp::Reverb lowReverb, lowMidReverb, midHighReverb, highReverb;
    juce::dsp::DelayLine<float> lowDelayLine{ 44100 * 2 }, lowMidDelayLine{ 44100 * 2 }, midHighDelayLine{ 44100 * 2 }, highDelayLine{ 44100 * 2 };
    juce::dsp::DryWetMixer<float> lowMixer, lowMidMixer, midHighMixer, highMixer;
    juce::dsp::Reverb::Parameters lowReverbParams, lowMidReverbParams, midHighReverbParams, highReverbParams;

    // --- Переменные для сглаженных значений реверба ---
    float currentLowWet = 0.0f, currentLowSpace = 0.5f, currentLowDistance = 0.5f, currentLowDelayMs = 0.0f;
    float currentLowMidWet = 0.0f, currentLowMidSpace = 0.5f, currentLowMidDistance = 0.5f, currentLowMidDelayMs = 0.0f;
    float currentMidHighWet = 0.0f, currentMidHighSpace = 0.5f, currentMidHighDistance = 0.5f, currentMidHighDelayMs = 0.0f;
    float currentHighWet = 0.0f, currentHighSpace = 0.5f, currentHighDistance = 0.5f, currentHighDelayMs = 0.0f;


    std::atomic<bool> isInternallySettingCrossoverParam{ false };

    juce::dsp::Gain<float> lowBandGainDSP, lowMidBandGainDSP, midHighBandGainDSP, highBandGainDSP;

    // Состояния Solo/Mute для DSP логики
    std::atomic<bool> low_isSoloed{ false }, lowMid_isSoloed{ false }, midHigh_isSoloed{ false }, high_isSoloed{ false };
    std::atomic<bool> anySoloActive{ false }; // Флаг, что хотя бы одна полоса солируется

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};
