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
    juce::AudioParameterFloat* midHighCrossover{ nullptr };

    // --- НОВЫЕ: Атомарные указатели на параметры реверба ---
    std::atomic<float>* lowWetParam = nullptr;
    std::atomic<float>* lowSpaceParam = nullptr;
    std::atomic<float>* lowDistanceParam = nullptr;
    std::atomic<float>* lowDelayParam = nullptr;
    std::atomic<float>* midWetParam = nullptr;
    std::atomic<float>* midSpaceParam = nullptr;
    std::atomic<float>* midDistanceParam = nullptr;
    std::atomic<float>* midDelayParam = nullptr;
    std::atomic<float>* highWetParam = nullptr;
    std::atomic<float>* highSpaceParam = nullptr;
    std::atomic<float>* highDistanceParam = nullptr;
    std::atomic<float>* highDelayParam = nullptr;
    // ----------------------------------------------------

    // Listener для параметров (если нужен)
    void parameterChanged(const juce::String& parameterID, float newValue) override;


    // --- Члены для Анализатора Спектра (стиль witte) ---
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    std::atomic<bool> nextFFTBlockReady{ false };
    juce::AbstractFifo abstractFifoInput{ fftSize * 2 };
    juce::AudioBuffer<float> audioFifoInput;
    juce::AbstractFifo abstractFifoOutput{ fftSize * 2 };
    juce::AudioBuffer<float> audioFifoOutput;
    void setCopyToFifo(bool _copyToFifo);
    // --------------------------------------------------

    // --- Члены для размера редактора (из witte) ---
    juce::Point<int> getSavedEditorSize() const { return editorSize; }
    void setSavedEditorSize(const juce::Point<int>& size) { editorSize = size; }
    // ---------------------------------------------
    bool isCopyToFifoEnabled() const { return copyToFifo.load(); }

    juce::AudioParameterBool* bypassParameter{ nullptr };
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;

    // --- Фильтры и обработка ---
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter leftLowMidLPF, rightLowMidLPF;
    Filter leftMidHighLPF, rightMidHighLPF;

    std::atomic<float> leftLowGain{ 1.f }, rightLowGain{ 1.f };
    std::atomic<float> leftMidGain{ 1.f }, rightMidGain{ 1.f };
    std::atomic<float> leftHighGain{ 1.f }, rightHighGain{ 1.f };

    juce::AudioBuffer<float> intermediateBuffer1;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    // --- Управление FIFO ---
    std::atomic<bool> copyToFifo{ false };
    void pushNextSampleToFifo(const juce::AudioBuffer<float>& buffer, int startChannel, int numChannels,
        juce::AbstractFifo& absFifo, juce::AudioBuffer<float>& fifo);

    float lastSampleRate = 44100.0f;
    juce::Point<int> editorSize = { 900, 700 }; // <-- Возможно, понадобится увеличить высоту

    void updateParameters(); // <-- Будем обновлять и параметры реверба здесь

    // --- НОВЫЕ: DSP объекты реверберации для каждой полосы ---
    juce::dsp::Reverb lowReverb, midReverb, highReverb;
    juce::dsp::DelayLine<float> lowDelayLine{ 44100 * 2 }, midDelayLine{ 44100 * 2 }, highDelayLine{ 44100 * 2 }; // Макс задержка 2 сек
    juce::dsp::DryWetMixer<float> lowMixer, midMixer, highMixer;
    juce::dsp::Reverb::Parameters lowReverbParams, midReverbParams, highReverbParams;
    // ---------------------------------------------------------

    // --- НОВЫЕ: Переменные для сглаженных значений реверба ---
    float currentLowWet = 0.0f, currentLowSpace = 0.5f, currentLowDistance = 0.5f, currentLowDelayMs = 0.0f;
    float currentMidWet = 0.0f, currentMidSpace = 0.5f, currentMidDistance = 0.5f, currentMidDelayMs = 0.0f;
    float currentHighWet = 0.0f, currentHighSpace = 0.5f, currentHighDistance = 0.5f, currentHighDelayMs = 0.0f;
    // ----------------------------------------------------------

    // НОВОЕ: Константа и флаг для логики кроссоверов
    static constexpr float MIN_CROSSOVER_SEPARATION = 10.0f; // Минимальное разделение в Гц (можно настроить)
    std::atomic<bool> isInternallySettingCrossoverParam{ false }; // Флаг для предотвращения рекурсии

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MBRPAudioProcessor)
};
